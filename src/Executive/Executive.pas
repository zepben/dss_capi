unit Executive;
{
  ----------------------------------------------------------
  Copyright (c) 2008-2015, Electric Power Research Institute, Inc.
  All rights reserved.
  ----------------------------------------------------------
}

{  Change Log

  8/12/99  Added Show Zone Help string

  10/11/99 Added Dump Commands option.  Moved ExecCommand into Public area.
  10/12/99 ADded new AutoAdd options and revised behavior of New Command.
  10/14/99 Added UE weighting option
           Fixed Redirect/Compile to change default directory.
  11/2/99  Added message in Open and Close cmd for ckt element not found.
  12/3/99  Fixed bug in command parser - needed quotes when rebuilding command line
  12/6/99  Merged Set and Solve commands
  1-14-00 Added Get Command
          Added LossWeight, UEreg, lossreg properties
  2-20-00 Revised Helpform so that help strings won't go away after Clear
  3-2-00  Repaired some places where re-parsing would mess up on names with blanks
  3-10-00 Added FileEdit and Export commands
  3-20-00 Added DefaultDaily and DefaultYearly Options
  4-17-00 Moved bulk of functions to ExecHelper
          Added AllocateLoads Command and AllocationFactors option
  8-23-00 Added Price Signal Option
  9-18-00 Fixed Dump Command Help
  9-20-00 Added Dynamic Mode
  10-3-00 Removed test for comment since '//' is now done in the Parser
  5/22/01 Changed behavior of Compile and Redirect with respect to directory changes.
  5/30/01 Add Set maxControlIterations
  7/19/01 Added Totals command, Capacity Command
  8/1/01  Revise the way the Capacity Command works
  9/12/02 Added Classes and UserClasses
  2/4/03  Added Set Bus=
          Added Zsc, Zsc012.
          Changed way Voltages command works

}

interface

USES
      Classes, DSSPointerList, Command, DSSClass;

TYPE
     TExecutive = class(TObject)
     private
         FRecorderOn: Boolean;
         FRecorderFile:String;
         RecorderFile: TFileStream;

         FUNCTION Get_LastError:String;
         FUNCTION Get_ErrorResult:Integer;


         function Get_Command: String;
         procedure Set_Command(const Value: String);
    procedure Set_RecorderOn(const Value: Boolean);

     public
         DSS: TDSSContext;
         constructor Create(dssContext: TDSSContext);
         destructor  Destroy; override;

         PROCEDURE CreateDefaultDSSItems;
         Procedure Write_to_RecorderFile(const s:string);

         Procedure Clear;
{$IFDEF DSS_CAPI_PM}
         Procedure ClearAll;
{$ENDIF}
         Property Command:String   read Get_Command write Set_Command;
         Property Error:Integer    read Get_ErrorResult;
         Property LastError:String read Get_LastError;
         Property RecorderOn:Boolean Read FRecorderOn write Set_RecorderOn;

     end;


implementation


USES ExecCommands, ExecOptions,
     {ExecHelper,} DSSClassDefs, DSSGlobals, ParserDel,  SysUtils,
     Utilities, Solution, DSSHelper,
     {$IFDEF FPC} CmdForms{$ELSE} DSSForms{$ENDIF};


//----------------------------------------------------------------------------
Constructor TExecutive.Create(dssContext: TDSSContext);
Begin
     Inherited Create;

     DSS := dssContext;
      
     // Exec Commands
     CommandList := TCommandList.Create(ExecCommand);

     // Exec options
     OptionList := TCommandList.Create(ExecOption);

     {Instantiate All DSS Classe Definitions, Intrinsic and User-defined}
     CreateDSSClasses(DSS);     // in DSSGlobals

     DSS.Circuits := TDSSPointerList.Create(2);   // default buffer for 2 active circuits
     DSS.NumCircuits := 0;
     DSS.ActiveCircuit := nil;

     DSS.LastCmdLine := '';
     DSS.RedirFile := '';

     FRecorderOn := FALSE;
     FrecorderFile := '';

     {Override Locale defaults so that CSV files get written properly}
     FormatSettings.DecimalSeparator  := '.';
     FormatSettings.ThousandSeparator := ',';

End;


//----------------------------------------------------------------------------
Destructor TExecutive.Destroy;

Begin
    If RecorderOn Then 
        RecorderOn := FALSE;

     ClearAllCircuits(DSS);

     CommandList.Free;
     OptionList.Free;
     DSS.Circuits.Free;

     DisposeDSSClasses(DSS);

     Inherited Destroy;
End;

//----------------------------------------------------------------------------
FUNCTION TExecutive.Get_LastError:String;

Begin
     Result := DSS.LastErrorMessage;
End;

//----------------------------------------------------------------------------
FUNCTION TExecutive.Get_ErrorResult:Integer;
Begin
     Result := DSS.ErrorNumber;
End;


//----------------------------------------------------------------------------
PROCEDURE TExecutive.CreateDefaultDSSItems;

{Create default loadshapes, growthshapes, and other general DSS objects
 used by all circuits.
}
Begin

{ this load shape used for generator dispatching, etc.   Loads may refer to it, also.}
   Command := 'new loadshape.default npts=24 1.0 mult=(.677 .6256 .6087 .5833 .58028 .6025 .657 .7477 .832 .88 .94 .989 .985 .98 .9898 .999 1 .958 .936 .913 .876 .876 .828 .756)';
   IF DSS.CmdResult = 0 THEN
   Begin
       Command := 'new growthshape.default 2 year="1 20" mult=(1.025 1.025)';  // 20 years at 2.5%
       Command := 'new spectrum.default 7  Harmonic=(1 3 5 7 9 11 13)  %mag=(100 33 20 14 11 9 7) Angle=(0 0 0 0 0 0 0)';
       Command := 'new spectrum.defaultload 7  Harmonic=(1 3 5 7 9 11 13)  %mag=(100 1.5 20 14 1 9 7) Angle=(0 180 180 180 180 180 180)';
       Command := 'new spectrum.defaultgen 7  Harmonic=(1 3 5 7 9 11 13)  %mag=(100 5 3 1.5 1 .7 .5) Angle=(0 0 0 0 0 0 0)';
       Command := 'new spectrum.defaultvsource 1  Harmonic=(1 )  %mag=(100 ) Angle=(0 ) ';
       Command := 'new spectrum.linear 1  Harmonic=(1 )  %mag=(100 ) Angle=(0 ) ';
       Command := 'new spectrum.pwm6 13  Harmonic=(1 3 5 7 9 11 13 15 17 19 21 23 25) %mag=(100 4.4 76.5 62.7 2.9 24.8 12.7 0.5 7.1 8.4 0.9 4.4 3.3) Angle=(-103 -5 28 -180 -33 -59 79 36 -253 -124 3 -30 86)';
       Command := 'new spectrum.dc6 10  Harmonic=(1 3 5 7 9 11 13 15 17 19)  %mag=(100 1.2 33.6 1.6 0.4 8.7  1.2  0.3  4.5 1.3) Angle=(-75 28 156 29 -91 49 54 148 -57 -46)';
       Command := 'New TCC_Curve.A 5 c_array=(1, 2.5, 4.5, 8.0, 14.)  t_array=(0.15 0.07 .05 .045 .045) ';
       Command := 'New TCC_Curve.D 5 c_array=(1, 2.5, 4.5, 8.0, 14.)  t_array=(6 0.7 .2 .06 .02)';
       Command := 'New TCC_Curve.TLink 7 c_array=(2 2.1 3 4 6 22 50)  t_array=(300 100 10.1 4.0 1.4 0.1  0.02)';
       Command := 'New TCC_Curve.KLink 6 c_array=(2 2.2 3 4 6 30)    t_array=(300 20 4 1.3 0.41 0.02)';
       Command := 'New "TCC_Curve.uv1547" npts=2 C_array=(0.5, 0.9, ) T_array=(0.166, 2, )';
       Command := 'New "TCC_Curve.ov1547" npts=2 C_array=(1.1, 1.2, ) T_array=(2, 0.166, )';
       Command := 'New "TCC_Curve.mod_inv" npts=15 C_array=(1.1, 1.3, 1.5, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 50, 100, ) T_array=(27.1053, 9.9029, 6.439, 3.8032, 2.4322, 1.9458, 1.6883, 1.5255, 1.4117, 1.3267, 1.2604, 1.2068, 0.9481, 0.7468, 0.6478, )';
       Command := 'New "TCC_Curve.very_inv" npts=15 C_array=(1.1, 1.3, 1.5, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 50, 100, ) T_array=(93.872, 28.9113, 16.179, 7.0277, 2.9423, 1.7983, 1.3081, 1.0513, 0.8995, 0.8023, 0.7361, 0.6891, 0.5401, 0.4988, 0.493, )';
       Command := 'New "TCC_Curve.ext_inv" npts=15 C_array=(1.1, 1.3, 1.5, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 50, 100, ) T_array=(134.4074, 40.9913, 22.6817, 9.5217, 3.6467, 2.0017, 1.2967, 0.9274, 0.7092, 0.5693, 0.4742, 0.4065, 0.1924, 0.133, 0.1245, )';
       Command := 'New "TCC_Curve.definite" npts=3 C_array=(1, 1.001, 100, ) T_array=(300, 1, 1, )';
   End;


End;


function TExecutive.Get_Command: String;
begin
    Result := DSS.LastCmdLine;
end;


procedure TExecutive.Set_Command(const Value: String);
{$IFDEF DSS_CAPI_PM}
var
    PMParent: TDSSContext;
    idx: Integer;
begin
    PMParent := DSS.GetPrime();
{$ELSE}
begin
{$ENDIF}
{$IFDEF DSS_CAPI_PM}
    if PMParent.AllActors then
    begin
        for idx := 0 to High(PMParent.Children) do
        begin
            //TODO: if in the future the commands are processed in threads, this would need a lock, and
            //      maybe allow certain commands only in the DSSPrime instance to simplify things
            if PMParent.AllActors then 
                ProcessCommand(PMParent.Children[idx], Value);
        end;
    end
    else
{$ENDIF}
        ProcessCommand(DSS, Value);
end;

procedure TExecutive.Clear;
begin
    IF (DSS.NumCircuits > 0) OR (DSS_CAPI_LEGACY_MODELS <> DSS_CAPI_LEGACY_MODELS_PREV) THEN
	Begin
    	if DSS.DIFilesAreOpen then
        	DSS.EnergyMeterClass.CloseAllDIFiles;

        {First get rid of all existing stuff}
        ClearAllCircuits(DSS);
        DisposeDSSClasses(DSS);

        {Now, Start over}
        CreateDSSClasses(DSS);
        CreateDefaultDSSItems;
    End;

    DSS.DefaultEarthModel     := DERI;
    DSS.LogQueries            := FALSE;
    DSS.MaxAllocationIterations := 2;

    {Prepare for new variables}
    DSS.ParserVars.Free;
    DSS.ParserVars := TParserVar.Create(100);  // start with space for 100 variables
    DSS.Parser.SetVars(DSS.ParserVars);
    DSS.AuxParser.SetVars(DSS.ParserVars);
end;

{$IFDEF DSS_CAPI_PM}
procedure TExecutive.ClearAll;
var
    PMParent: TDSSContext;
    i: integer;
begin
    PMParent := DSS.GetPrime();
    
    for i := 1 to high(PMParent.Children) do
    begin
        PMParent.Children[i].Free;
    end;
    SetLength(PMParent.Children, 1);
    PMParent.ActiveChildIndex := 0;
    PMParent.ActiveChild := PMParent;
    PMParent.DSSExecutive.Clear();
end;
{$ENDIF}

procedure TExecutive.Set_RecorderOn(const Value: Boolean);
begin
    If Value Then 
    Begin
        If Not FRecorderOn Then 
        Begin
            FRecorderFile := DSS.OutputDirectory + 'DSSRecorder.DSS' ;
            RecorderFile := TFileStream.Create(FRecorderFile, fmCreate);
        End
        else
        begin
            RecorderFile.Free();
            RecorderFile := TFileStream.Create(FRecorderFile, fmCreate);
        end;
    End 
    Else If FRecorderOn Then 
    Begin
        FreeAndNil(RecorderFile);
    End;
    DSS.GlobalResult := FRecorderFile;
    FRecorderOn := Value;
end;

procedure TExecutive.Write_to_RecorderFile(const s: string);
begin
   FSWriteln(Recorderfile, S);
end;

end.

