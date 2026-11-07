class IdleAI extends AIInfo {
	function GetAuthor()      { return "junction-tool"; }
	function GetName()        { return "IdleAI"; }
	function GetDescription() { return "Does nothing; only gives the test rig a company."; }
	function GetVersion()     { return 1; }
	function GetDate()        { return "2026-11-07"; }
	function CreateInstance() { return "IdleAI"; }
	function GetShortName()   { return "IDLE"; }
	function GetAPIVersion()  { return "16"; }
}

RegisterAI(IdleAI());
