class JunctionTestGS extends GSInfo {
	function GetAuthor()      { return "junction-tool"; }
	function GetName()        { return "JunctionTestGS"; }
	function GetDescription() { return "Test rig: build simple rail and run trains via admin-port JSON"; }
	function GetVersion()     { return 2; }
	function GetDate()        { return "2026-11-07"; }
	function CreateInstance() { return "JunctionTestGS"; }
	function GetShortName()   { return "JTST"; }
	function GetAPIVersion()  { return "16"; }
}

RegisterGS(JunctionTestGS());
