class IdleAI extends AIController {
	function Start()
	{
		while (true) AIController.Sleep(1000);
	}
}
