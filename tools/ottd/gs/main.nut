/* Spike GameScript: answers {"id":N,"cmd":"ping"} with {"id":N,"pong":true,"date":D}. */
class JunctionSpikeGS extends GSController {
	function Start()
	{
		GSController.Sleep(1);
		while (true) {
			while (GSEventController.IsEventWaiting()) {
				local ev = GSEventController.GetNextEvent();
				if (ev == null) continue;
				if (ev.GetEventType() == GSEvent.ET_ADMIN_PORT) {
					this.Handle(GSEventAdminPort.Convert(ev).GetObject());
				}
			}
			GSController.Sleep(1);
		}
	}

	function Handle(msg)
	{
		if (typeof(msg) != "table" || !("cmd" in msg)) return;
		local id = ("id" in msg) ? msg.id : 0;
		if (msg.cmd == "ping") {
			GSAdmin.Send({ id = id, pong = true, date = GSDate.GetCurrentDate() });
		} else {
			GSAdmin.Send({ id = id, error = "unknown cmd" });
		}
	}
}
