unit CktTree;

{
  ----------------------------------------------------------
  Copyright (c) 2008-2015, Electric Power Research Institute, Inc.
  All rights reserved.
  ----------------------------------------------------------
}

interface

uses
    Classes,
    ArrayDef,
    StackDef,
    DSSPointerList,
    CktElement;

type
    TAdjArray = array of TList;

    TCktTreeNode = class(TObject)
    PRIVATE
        FChildBranches: TDSSPointerList;  // List of CktTreeNode pointers

        NumToBuses, ToBusPtr: Integer;
        ToBusList: pIntegerArray;

        function Get_FirstChild: TCktTreeNode; inline;
        function Get_NextChild: TCktTreeNode; inline;
        function Get_Parent: TCktTreeNode; inline;
        procedure Set_AddChild(const Value: TCktTreeNode); inline;
        function Get_NumChildren: Integer; inline;
        function Get_NumObjects: Integer; inline;
        function Get_ToBusReference: Integer; inline;
        procedure Set_ToBusReference(const Value: Integer); inline;
        function Get_FirstObject: Pointer; inline;
        function Get_NextObject: Pointer; inline;

    PROTECTED
        ChildAdded: Boolean;
        LexicalLevel: Integer;
        FParentBranch: TCktTreeNode;
        FShuntObjects: TDSSPointerList;  // Generic objects attached to the tree at this node

    PUBLIC
        CktObject: Pointer;    // Pointer to the circuit object referenced
        FromBusReference: Integer;
        VoltBaseIndex: Integer;
        FromTerminal: Integer;
        IsLoopedHere, IsParallel, IsDangling: Boolean;
        LoopLineObj: Pointer;

        constructor Create(const pParent: TCktTreeNode; const pSelfObj: Pointer);
        destructor Destroy; OVERRIDE;

        procedure ResetToBusList;
        property AddChildBranch: TCktTreeNode WRITE Set_AddChild;
        procedure AddShuntObject(Value: Pointer);
        property FirstChildBranch: TCktTreeNode READ Get_FirstChild;
        property NextChildBranch: TCktTreeNode READ Get_NextChild;
        property FirstShuntObject: Pointer READ Get_FirstObject;
        property NextShuntObject: Pointer READ Get_NextObject;

        property ParentBranch: TCktTreeNode READ Get_Parent;
        property NumChildBranches: Integer READ Get_NumChildren;  // Number of children at present node
        property NumShuntObjects: Integer READ Get_NumObjects; // Number of objects at present node
        property ToBusReference: Integer READ Get_ToBusReference WRITE Set_ToBusReference;
    end;


    TZoneEndsList = class(Tobject)
    PRIVATE
        EndNodeList: TDSSPointerList;
        EndBuses: pIntegerArray;

    PUBLIC
        NumEnds: Integer;

        constructor Create;
        destructor Destroy; OVERRIDE;

        procedure Add(const Node: TCktTreeNode; EndBusRef: Integer);
        function Get(i: Integer; var Node: TCktTreeNode): Integer;
    end;


    TCktTree = class(TObject)
    PRIVATE
        FirstNode: TCktTreeNode;

        ForwardStack: TPstack;

        function Get_Forward: Pointer;
        function Get_Backward: Pointer;
        function Get_First: Pointer;
        function Get_Parent: Pointer;
        function Get_FirstObject: Pointer;
        function Get_NextObject: Pointer;
        function Get_Active: Pointer;
        function Get_Level: Integer;

        procedure Set_Active(p: Pointer);  // Set present node to this value
        procedure PushAllChildren;
    PUBLIC
        PresentBranch: TCktTreeNode;
        ZoneEndsList: TZoneEndsList;

        constructor Create;
        destructor Destroy; OVERRIDE;

        procedure StartHere;   // Start Forward Search at the present location
                              // can also use active
        procedure AddNewChild(Value: Pointer; BusRef, TerminalNo: Integer);
        procedure Add(Value: Pointer); // Adds Child and makes it present -- previously "New"
       //Property NewChild  :Pointer Write Set_NewChild; // Adds child to present, but doesn't change present
        
        procedure AddNewObject(Value: Pointer); // Adds a pointer to an object to be associated with the current node
        property First: Pointer READ Get_First;  // Returns pointer to first cktobject
        property Parent: Pointer READ Get_Parent;
        property FirstObject: Pointer READ Get_FirstObject;
        property NextObject: Pointer READ Get_NextObject;
        property GoForward: Pointer READ Get_Forward;
        property GoBackward: Pointer READ Get_Backward;
        property Active: Pointer READ Get_Active WRITE Set_Active;
        property Level: Integer READ Get_Level;  {Get lexical level of present node}
    end;

   // build a tree of connected elements beginning at StartElement
   // Analyze = TRUE will check for loops, isolated components, and parallel lines (takes longer)
function GetIsolatedSubArea(Circuit: TObject; StartElement: TDSSCktElement; Analyze: Boolean = FALSE): TCktTree;
procedure BuildActiveBusAdjacencyLists(Circuit: TObject; var lstPD, lstPC: TAdjArray);
procedure FreeAndNilBusAdjacencyLists(var lstPD, lstPC: TAdjArray);

implementation

uses
    Circuit,
    PDElement,
    PCElement,
    DSSGlobals,
    Utilities,
    DSSClass,
    DSSHelper;

constructor TcktTreeNode.Create(const pParent: TCktTreeNode; const pSelfobj: Pointer);

begin
    inherited create;
    CktObject := pSelfObj;
    FParentBranch := pParent;
    if FParentBranch <> NIL then
        LexicalLevel := FParentBranch.LexicalLevel + 1
    else
        LexicalLevel := 0;
    FChildBranches := TDSSPointerList.Create(2);
    FShuntObjects := TDSSPointerList.Create(1);
    FromBusReference := 0;
    VoltBaseIndex := 0; // Index to voltage base list used by energymeter and maybe others
    NumToBuses := 0;
    ToBusList := NIL;
    ToBusPtr := 0;
    ChildAdded := FALSE;
    IsDangling := TRUE;
    IsLoopedHere := FALSE;
    IsParallel := FALSE;
    LoopLineObj := NIL;
end;

destructor TcktTreeNode.Destroy;
var
    pChild, pNext: Pointer;
    TempNode: TCktTreeNode;
begin
    pChild := FChildBranches.First;
    while pChild <> NIL do
    begin
        pNext := FChildBranches.Next;
        TempNode := TcktTreeNode(pChild);
        TempNode.Free;
        pChild := pNext;
    end;
    Reallocmem(ToBusList, 0);
    FChildBranches.Free;
    FShuntObjects.Free;
    inherited Destroy;
end;

procedure TcktTreeNode.Set_AddChild(const Value: TCktTreeNode);
begin
    FChildBranches.Add(Value);
    ChildAdded := TRUE;
end;

procedure TcktTreeNode.AddShuntObject(Value: Pointer);
begin
    FShuntObjects.Add(Value);
end;

function TcktTreeNode.Get_FirstChild: TCktTreeNode;
begin
    Result := FChildBranches.First;
end;

function TcktTreeNode.Get_NextChild: TCktTreeNode;
begin
    Result := FChildBranches.Next;
end;

function TcktTreeNode.Get_Parent: TCktTreeNode;
begin
    Result := FParentBranch;
end;

constructor TcktTree.Create;
begin
    inherited create;
    FirstNode := NIL;
    PresentBranch := NIL;
    ZoneEndsList := TZoneEndsList.Create;
    ForwardStack := Tpstack.Create(200);
end;

destructor TcktTree.Destroy;
begin
    ForwardStack.Free;
    if assigned(ZoneEndsList) then
        ZoneEndsList.Free;
    if Assigned(FirstNode) then
        FirstNode.Free;
    inherited Destroy;
end;

procedure TcktTree.Add(Value: Pointer);
begin
    PresentBranch := TcktTreeNode.Create(PresentBranch, Value);
    if FirstNode = NIL then
        FirstNode := PresentBranch;
end;

procedure TcktTree.AddNewChild(Value: Pointer; BusRef, TerminalNo: Integer);
var
    TempNode: TCktTreeNode;
begin
    if PresentBranch = NIL then
    begin
        Add(Value);
    end
    else
    begin
        TempNode := TcktTreeNode.Create(PresentBranch, Value);
        with TempNode do
        begin
            FromBusReference := BusRef;
            FromTerminal := TerminalNo;
        end;

        PresentBranch.AddChildBranch := TempNode;
    end;
end;

procedure TcktTree.AddNewObject(Value: Pointer);
begin
    if PresentBranch <> NIL then
    begin
        PresentBranch.AddShuntObject(Value);
    end;
end;

procedure TcktTree.PushAllChildren;
var
    pChild: Pointer;
begin
    if PresentBranch = NIL then
        Exit;

    // Push all children of present node onto stack
    pChild := PresentBranch.FirstChildBranch;
    while pChild <> NIL do
    begin
        ForwardStack.Push(pChild);
        pChild := PresentBranch.NextChildBranch;
    end;
    PresentBranch.ChildAdded := FALSE;
end;

function TcktTree.Get_Forward: Pointer;
begin
    // MoveForward from Present node

    // If we have added children to the present node since we opened it push em on
    if PresentBranch <> NIL then
        if PresentBranch.ChildAdded then
            PushAllChildren;

    // If the forward stack is empty push stuff on it to get started
    if ForwardStack.Size = 0 then
        PushAllChildren;

    PresentBranch := ForwardStack.Pop;
    PushAllChildren;   // push all children of latest
    if PresentBranch <> NIL then
        Result := PresentBranch.CktObject
    else
        Result := NIL;
end;

function TcktTree.Get_Backward: Pointer;
begin
    if PresentBranch = NIL then
    begin
        Result := NIL;
        Exit;
    end;
    // Move Backwardfrom Present node and reset forward stack
    PresentBranch := PresentBranch.ParentBranch;
    ForwardStack.Clear;
    if PresentBranch <> NIL then
        Result := PresentBranch.CktObject
    else
        Result := NIL;
end;

function TcktTree.Get_Parent: Pointer;
begin
    if PresentBranch = NIL then
    begin
        Result := NIL;
        Exit;
    end;

    if PresentBranch.FParentBranch <> NIL then
        Result := PresentBranch.FParentBranch.CktObject
    else
        Result := NIL;
end;

function TcktTree.Get_First: Pointer;
begin
    // go to beginning and reset forward stack
    PresentBranch := FirstNode;
    ForwardStack.Clear;
    PushAllChildren;
    if PresentBranch <> NIL then
        Result := PresentBranch.CktObject
    else
        Result := NIL;
end;

function TcktTree.Get_FirstObject: Pointer;
begin
    if PresentBranch <> NIL then
        Result := PresentBranch.FShuntObjects.First
    else
        Result := NIL;
end;

function TcktTree.Get_NextObject: Pointer;
begin
    if PresentBranch <> NIL then
        Result := PresentBranch.FShuntObjects.Next
    else
        Result := NIL;
end;

function TcktTree.Get_Active: Pointer;
begin
    if PresentBranch <> NIL then
        Result := PresentBranch.CktObject
    else
        Result := NIL;
end;

procedure TcktTree.Set_Active(p: Pointer);
var
    Temp: Pointer;
begin
    Temp := Get_First;
    while Temp <> NIL do
    begin
        if PresentBranch.CktObject = p then
            Break;
        Temp := Get_Forward;
    end;

    ForwardStack.Clear;
end;

procedure TcktTree.StartHere;
begin
    ForwardStack.Clear;
    if PresentBranch <> NIL then
        ForwardStack.Push(PresentBranch);
end;

function TcktTree.Get_Level: Integer;

begin
    if PresentBranch <> NIL then
        result := PresentBranch.LexicalLevel
    else
        result := 0;
end;


function TCktTreeNode.Get_NumChildren: Integer;
begin
    Result := FChildBranches.Count;
end;

function TCktTreeNode.Get_NumObjects: Integer;
begin
    Result := FShuntObjects.Count;
end;


procedure TZoneEndsList.Add(const Node: TCktTreeNode; EndBusRef: Integer);
begin
    Inc(NumEnds);
    EndnodeList.Add(Node);
    Reallocmem(EndBuses, Sizeof(EndBuses) * NumEnds);
    EndBuses^[NumEnds] := EndBusRef;
end;

constructor TZoneEndsList.Create;
begin
    EndnodeList := TDSSPointerList.Create(10);
    NumEnds := 0;
    EndBuses := NIL;
end;

destructor TZoneEndsList.Destroy;
begin
    EndnodeList.Free;
    Reallocmem(EndBuses, 0);
    inherited;
end;

function TZoneEndsList.Get(i: Integer; var Node: TCktTreeNode): Integer;
begin
    Node := EndnodeList.Get(i);
    Result := EndBuses^[i];
end;

function TCktTreeNode.Get_ToBusReference: Integer;
{Sequentially access the To Bus list if more than one with each invocation of the property}
begin
    if NumToBuses = 1 then
    begin
        Result := ToBusList^[1];  // Always return the first
    end
    else
    begin
        Inc(ToBusPtr);
        if ToBusPtr > NumToBuses then
        begin
            Result := -1;
            ToBusPtr := 0;  // Ready for next sequence of access
        end
        else
            Result := ToBusList^[ToBusPtr];
    end;
end;

procedure TCktTreeNode.Set_ToBusReference(const Value: Integer);
begin
    Inc(NumToBuses);
    Reallocmem(ToBusList, Sizeof(ToBusList^[1]) * NumToBuses);
    TobusList^[NumToBuses] := Value;
end;

procedure TCktTreeNode.ResetToBusList;
begin
    ToBusPtr := 0;
end;

function TCktTreeNode.Get_FirstObject: Pointer;
begin
    Result := FShuntObjects.First;
end;

function TCktTreeNode.Get_NextObject: Pointer;
begin
    Result := FShuntObjects.Next;
end;

////////////////////////////////////////////////////////////////////////
//
// utility code for building a connected tree starting from a circuit element
//
////////////////////////////////////////////////////////////////////////

// sources are excluded from the PC element list, so this is a brute-force search
procedure GetSourcesConnectedToBus(Ckt: TDSSCircuit; BusNum: Integer; BranchList: TCktTree; Analyze: Boolean);
var
    psrc: TPCElement;      // Sources are special PC elements
begin
    with Ckt do
    begin
        for psrc in Sources do
        begin
            if not psrc.Enabled then
                continue;

            if Analyze or (not (Flg.Checked in psrc.Flags)) then
            begin
                if (psrc.Terminals[0].BusRef = BusNum) then
                begin  // ?Connected to this bus ?
                    if Analyze then
                    begin
                        Exclude(psrc.Flags, Flg.IsIsolated);
                        BranchList.PresentBranch.IsDangling := FALSE;
                    end;
                    if not (Flg.Checked in psrc.Flags) then
                    begin
                        BranchList.AddNewObject(psrc);
                        Include(psrc.Flags, Flg.Checked);
                    end;
                end;
            end;
        end;
    end;
end;

procedure GetPCElementsConnectedToBus(adjLst: TList; BranchList: TCktTree; Analyze: Boolean);
var
    p: TDSSCktElement;
    i: Integer;
begin
    for i := 0 to adjLst.Count - 1 do
    begin
        p := adjLst[i];
        if p.Enabled then
        begin
            if Analyze then
            begin
                Exclude(p.Flags, Flg.IsIsolated);
                BranchList.PresentBranch.IsDangling := FALSE;
            end;
            if not (Flg.Checked in p.Flags) then
            begin
                BranchList.AddNewObject(p);
                Include(p.Flags, Flg.Checked);
            end;
        end;
    end;
end;

procedure FindAllChildBranches(adjLst: TList; BusNum: Integer; BranchList: TCktTree;
    Analyze: Boolean; ActiveBranch: TDSSCktElement);
var
    i, j: Integer;
    p: TDSSCktElement;
begin
    for i := 0 to adjLst.Count - 1 do
    begin
        p := adjLst[i];
        if p.Enabled and not (p = ActiveBranch) then
        begin
            if Analyze or (not (Flg.Checked in p.Flags)) then
            begin
                if (not IsShuntElement(p)) and AllTerminalsClosed(p) then
                begin
                    for j := 1 to p.NTerms do
                    begin
                        if BusNum = p.Terminals[j - 1].BusRef then
                        begin
                            if Analyze then
                            begin
                                Exclude(p.Flags, Flg.IsIsolated);
                                BranchList.PresentBranch.IsDangling := FALSE;
                                if (Flg.Checked in p.Flags) and (BranchList.Level > 0) then
                                begin
                                    BranchList.PresentBranch.IsLoopedHere := TRUE;
                                    BranchList.PresentBranch.LoopLineObj := p;
                                    if IsLineElement(p) and IsLineElement(ActiveBranch) then
                                        if CheckParallel(ActiveBranch, p) then
                                            BranchList.PresentBranch.IsParallel := TRUE;
                                end;
                            end;
                            if not (Flg.Checked in p.Flags) then
                            begin
                                BranchList.AddNewChild(p, BusNum, j);
                                p.TerminalsChecked[j - 1] := TRUE;
                                Include(p.Flags, Flg.Checked);
                                Break; // For
                            end;
                        end;
                    end;
                end;
            end;
        end;
    end;
end;

procedure GetShuntPDElementsConnectedToBus(adjLst: TList; BranchList: TCktTree; Analyze: Boolean);
var
    p: TDSSCktElement;
    i: Integer;
begin
    for i := 0 to adjLst.Count - 1 do
    begin
        p := adjLst[i];
        if p.Enabled and IsShuntElement(p) then
        begin
            if Analyze then
            begin
                Exclude(p.Flags, Flg.IsIsolated);
                BranchList.PresentBranch.IsDangling := FALSE;
            end;
            if not (Flg.Checked in p.Flags) then
            begin
                BranchList.AddNewObject(p);
                Include(p.Flags, Flg.Checked);
            end;
        end;
    end;
end;

function GetIsolatedSubArea(Circuit: TObject; StartElement: TDSSCktElement; Analyze: Boolean): TCktTree;
var
    TestBusNum: Integer;
    BranchList: TCktTree;
    iTerm: Integer;
    TestBranch,
    TestElement: TDSSCktElement;
    lstPD, lstPC: TAdjArray;
    Ckt: TDSSCircuit;
begin
    Ckt := TDSSCircuit(Circuit);
    lstPD := Ckt.GetBusAdjacentPDLists;
    lstPC := Ckt.GetBusAdjacentPCLists;

    BranchList := TCktTree.Create;
    TestElement := StartElement;

    BranchList.Add(TestElement);
    if Analyze then
        Exclude(TestElement.Flags, Flg.IsIsolated);
    // TestElement.LastTerminalChecked := 0;  // We'll check things connected to both sides

    // Check off this element so we don't use it again
    Include(TestElement.Flags, Flg.Checked);

    // Now start looking for other branches
    // Finds any branch connected to the TestBranch and adds it to the list
    // Goes until end of circuit, another energy meter, an open terminal, or disabled device.
    TestBranch := TestElement;
    while TestBranch <> NIL do
    begin
        for iTerm := 1 to TestBranch.Nterms do
        begin
            if not TestBranch.TerminalsChecked[iTerm - 1] then
            begin
                // Now find all pc Elements connected to the bus on this end of branch
                // attach them as generic objects to cktTree node.
                TestBusNum := TestBranch.Terminals[iTerm - 1].BusRef;
                BranchList.PresentBranch.ToBusReference := TestBusNum;   // Add this as a "to" bus reference
                if TestBusNum > 0 then
                begin
                    Ckt.Buses[TestBusNum].BusChecked := TRUE;
                    GetSourcesConnectedToBus(Ckt, TestBusNum, BranchList, Analyze);
                    GetPCElementsConnectedToBus(lstPC[TestBusNum], BranchList, Analyze);
                    GetShuntPDElementsConnectedToBus(lstPD[TestBusNum], BranchList, Analyze);
                    FindAllChildBranches(lstPD[TestBusNum], TestBusNum, BranchList, Analyze, TestBranch);
                end;
            end;
        end;
        TestBranch := BranchList.GoForward;
    end;
    Result := BranchList;
end;

procedure BuildActiveBusAdjacencyLists(Circuit: TObject; var lstPD, lstPC: TAdjArray);
var
    i, j, nBus: Integer;
    pCktElement: TDSSCktElement;
    Ckt: TDSSCircuit;
begin
    Ckt := TDSSCircuit(Circuit);
    nBus := Ckt.NumBuses;
    // Circuit.Buses is effectively 1-based; bus 0 is ground
    SetLength(lstPD, nBus + 1);
    SetLength(lstPC, nBus + 1);
    for i := 0 to nBus do
    begin
        lstPD[i] := TList.Create; // default capacity should be enough
        lstPC[i] := TList.Create;
    end;

    for pCktElement in Ckt.PCElements do
    begin
        if pCktElement.Enabled then
        begin
            i := pCktElement.Terminals[0].BusRef;
            lstPC[i].Add(pCktElement);
        end;
    end;

    for pCktElement in Ckt.PDElements do
    // Put only eligible PDElements in the list
    begin
        if pCktElement.Enabled then
            if IsShuntElement(pCktElement) then
            begin
                i := pCktElement.Terminals[0].BusRef;
                lstPC[i].Add(pCktElement);
            end
            else
            if AllTerminalsClosed(pCktElement) then
                for j := 1 to pCktElement.Nterms do
                begin
                    i := pCktElement.Terminals[j - 1].BusRef;
                    lstPD[i].Add(pCktElement);
                end;
    end;
end;

procedure FreeAndNilBusAdjacencyLists(var lstPD, lstPC: TAdjArray);
var
    i: Integer;
begin
    for i := Low(lstPD) to High(lstPD) do
    begin
        lstPD[i].Free;
        lstPC[i].Free;
    end;
    SetLength(lstPD, 0);
    SetLength(lstPC, 0);
    lstPD := NIL;
    lstPC := NIL;
end;

end.
