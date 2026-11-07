class JunctionSpikeGS extends GSInfo {
	function GetAuthor()      { return "junction-tool"; }
	function GetName()        { return "JunctionSpikeGS"; }
	function GetDescription() { return "Spike: answers admin-port JSON pings"; }
	function GetVersion()     { return 1; }
	function GetDate()        { return "2026-11-07"; }
	function CreateInstance() { return "JunctionSpikeGS"; }
	function GetShortName()   { return "JSPK"; }
	function GetAPIVersion()  { return "16"; }
}

RegisterGS(JunctionSpikeGS());
