unit NamedObject;

{
  ----------------------------------------------------------
  Copyright (c) 2009-2022, Electric Power Research Institute, Inc.
  All rights reserved.
  ----------------------------------------------------------
}

interface

type

    TUuid = TGuid;    // this is a GUID compliant to RFC 4122, v4

    TNamedObject = class(TObject)
    // TODO: remove TNamedObject as a whole. Use an extra structure to track the data here.
    PROTECTED
        pUuid: ^TUuid;  // compliant to RFC 4122, v4
        LName: String;  // localName is unique within a class, like the old FName
    PRIVATE
        function Get_UUID: TUuid;
        function Get_ID: String;
        function Get_CIM_ID: String;
        procedure Set_UUID(const Value: TUuid);
    PUBLIC
        DisplayName: String;

        constructor Create(ClassName_: String);
        destructor Destroy; OVERRIDE;
        
        property LocalName: String READ LName WRITE LName;
        property UUID: TUuid READ Get_UUID WRITE Set_UUID;
        property ID: String READ Get_ID;
        property CIM_ID: String READ Get_CIM_ID;
    end;

function CreateUUID4(out UUID: TUuid): Integer;
function StringToUUID(const S: String): TUuid;
function UUIDToString(const UUID: TUuid): String;
function UUIDToCIMString(UUID: TUuid): String;

implementation

uses
    Sysutils,
    StrUtils;

function CreateUUID4(out UUID: TUuid): Integer;
begin
    Result := CreateGUID(UUID);
    UUID.D3 := (UUID.D3 and $0fff) or $4000;   // place a 4 at character 13
    UUID.D4[0] := (UUID.D4[0] and $3f) or $80; // character 17 to be 8, 9, A or B
end;

function StringToUUID(const S: String): TUuid;
begin
    Result := StringToGUID(S);
end;

function UUIDToString(const UUID: TUuid): String;
begin
    Result := GuidToString(UUID);
end;

function UUIDToCIMString(UUID: TUuid): String;
var
    s: String;
begin
    s := GUIDToString(UUID);
    Result := MidStr(s, 2, Length(s) - 2);
end;

constructor TNamedObject.Create(ClassName_: String);
begin
    inherited Create;
    LName := '';
    DisplayName := '';
    pUuid := NIL;
end;

destructor TNamedObject.Destroy;
begin
    if pUuid <> NIL then
        Dispose(pUuid);
    inherited Destroy;
end;

procedure TNamedObject.Set_UUID(const Value: TUuid);
begin
    if pUuid = NIL then
        New(pUuid);
    pUuid^ := Value;
end;

function TNamedObject.Get_UUID: TUuid;
begin
    if pUuid = NIL then
    begin
        New(pUuid);
        CreateUUID4(pUuid^);
    end;
    Result := pUuid^;
end;

function TNamedObject.Get_ID: String;
begin
    Result := GUIDToString(Get_UUID);
end;

function TNamedObject.Get_CIM_ID: String;
begin
    Result := UUIDToCIMString(Get_UUID);
end;

end.
