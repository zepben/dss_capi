UNIT CAPI_DSSProperty;
{$inline on}

INTERFACE

USES CAPI_Utils;

function DSSProperty_Get_Description():PAnsiChar;cdecl;
function DSSProperty_Get_Name():PAnsiChar;cdecl;
function DSSProperty_Get_Val():PAnsiChar;cdecl;
procedure DSSProperty_Set_Val(const Value: PAnsiChar);cdecl;
procedure DSSProperty_Set_Index(const Value: Integer);cdecl;
procedure DSSProperty_Set_Name(const Value: PAnsiChar);cdecl;

IMPLEMENTATION

USES CAPI_Constants, CAPI_Globals, DSSClass, DSSGlobals, Executive, SysUtils;

function DSSProperty_Get_Description_AnsiString():AnsiString;inline;
begin
      Result := '';
      If (ActiveCircuit<> Nil) and (FPropIndex <> 0) {and (FPropClass <> Nil)} Then
      With  ActiveDSSObject.ParentClass Do
        If FPropIndex <= NumProperties Then
          Result := PropertyHelp^[FPropIndex];

end;

function DSSProperty_Get_Description():PAnsiChar;cdecl;
begin
    Result := DSS_GetAsPAnsiChar(DSSProperty_Get_Description_AnsiString());
end;
//------------------------------------------------------------------------------
function DSSProperty_Get_Name_AnsiString():AnsiString;inline;
begin
      Result := '';
      If (ActiveCircuit<> Nil) and (FPropIndex <> 0) {and (FPropClass <> Nil)} Then
        With  ActiveDSSObject.ParentClass   Do
        If FPropIndex <= NumProperties Then
          Result := PropertyName^[FPropIndex];

end;

function DSSProperty_Get_Name():PAnsiChar;cdecl;
begin
    Result := DSS_GetAsPAnsiChar(DSSProperty_Get_Name_AnsiString());
end;
//------------------------------------------------------------------------------
function DSSProperty_Get_Val_AnsiString():AnsiString;inline;
begin
    Result := '';
    If ActiveCircuit = Nil then Exit;
    With ActiveDSSObject Do
    begin
        If FPropIndex <= ParentClass.NumProperties Then
            Result := PropertyValue[ParentClass.PropertyIdxMap[FPropIndex]];
    end;
end;

function DSSProperty_Get_Val():PAnsiChar;cdecl;
begin
    Result := DSS_GetAsPAnsiChar(DSSProperty_Get_Val_AnsiString());
end;
//------------------------------------------------------------------------------
procedure DSSProperty_Set_Val(const Value: PAnsiChar);cdecl;
begin
    If ActiveCircuit = Nil then Exit;
    
    With ActiveDSSObject Do
    begin
        If (FPropIndex > ParentClass.NumProperties) or (FPropIndex < 1) Then
        begin
            DoSimpleMsg(Format(
                'Invalid property index %d for "%s.%s"', 
                [FPropIndex, ParentClass.Name, Name]
            ), 33001);
            Exit;
        end;
        DSSExecutive.Command := 
            'Edit ' + ParentClass.Name + '.' + Name + ' ' + 
            ParentClass.PropertyName^[FPropIndex] + '=' + String(Value);
    end;
End;
//------------------------------------------------------------------------------
procedure DSSProperty_Set_Index(const Value: Integer);cdecl;
begin
  If ActiveCircuit <> Nil Then
  Begin
     FPropIndex := Value + 1;
  End;
end;
//------------------------------------------------------------------------------
procedure DSSProperty_Set_Name(const Value: PAnsiChar);cdecl;
var i: integer;
begin
  If ActiveCircuit <> Nil Then
  Begin
    FPropClass := ActiveDSSObject.ParentClass;
    FPropIndex := 0;
    If FPropClass <> Nil Then
     With FPropClass Do
     For i := 1 to NumProperties Do Begin
         If CompareText(Value, PropertyName^[i]) = 0 Then Begin
             FPropIndex := i;
             Break;
         End;
     End;
  End;
end;
//------------------------------------------------------------------------------
END.
