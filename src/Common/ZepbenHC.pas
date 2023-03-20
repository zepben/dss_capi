unit ZepbenHC;

{$MACRO ON}
{$DEFINE RMQPUSH_CALL:=cdecl;external}

interface
uses
    SysUtils;


procedure send_di_as_double(t:integer;mrid:String;year:Double;data:array of Double;fieldnames:array of String;len:integer);RMQPUSH_CALL;
procedure send_di_data(t:integer;mrid:String;year:Double;data:array of String;fieldnames:array of String;len:integer);RMQPUSH_CALL;
    
procedure close_queue();RMQPUSH_CALL;

function getBooleanEnv(name: String; default: boolean): Boolean; inline; 
procedure debug(msg: String); 


implementation

function getBooleanEnv(name: String; default: boolean): Boolean; inline; 
begin
    Result := Length(SysUtils.GetEnvironmentVariable(name)) > 0;
end;

procedure debug(msg: String); 
begin
   if (Length(SysUtils.GetEnvironmentVariable('HC_DEBUG')) > 0) then
       writeln(msg);
end;

end.
