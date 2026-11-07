/* JunctionTestGS: JSON request/reply over the admin port. Every request has "id" and "cmd". */
class JunctionTestGS extends GSController {
	company = -1;

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

	/* ---- JSON encoding / chunked replies ---- */
	function Esc(s)
	{
		local r = "";
		foreach (c in s) {
			local ch = c.tochar();
			if (ch == "\"") r += "\\\"";
			else if (ch == "\\") r += "\\\\";
			else if (c < 32) r += " ";
			else r += ch;
		}
		return r;
	}

	function Json(v)
	{
		switch (typeof(v)) {
			case "null": return "null";
			case "bool": return v ? "true" : "false";
			case "integer": return v.tostring();
			case "string": return "\"" + this.Esc(v) + "\"";
			case "array": {
				local parts = [];
				foreach (x in v) parts.append(this.Json(x));
				return "[" + this.Join(parts, ",") + "]";
			}
			case "table": {
				local parts = [];
				foreach (k, x in v) parts.append("\"" + this.Esc(k) + "\":" + this.Json(x));
				return "{" + this.Join(parts, ",") + "}";
			}
		}
		return "null";
	}

	function Join(parts, sep)
	{
		local r = "";
		foreach (i, p in parts) r += (i > 0 ? sep : "") + p;
		return r;
	}

	function Reply(id, tbl)
	{
		tbl.id <- id;
		local text = this.Json(tbl);
		if (text.len() <= 1400) {
			GSAdmin.Send(tbl);
			return;
		}
		local piece = 600;
		local n = (text.len() + piece - 1) / piece;
		for (local i = 0; i < n; i++) {
			GSAdmin.Send({ id = id, chunk = i, of = n, data = text.slice(i * piece, min(text.len(), (i + 1) * piece)) });
		}
	}

	function Err(id, msg) { this.Reply(id, { error = msg }); }

	/* ---- helpers ---- */
	function Tile(x, y) { return GSMap.GetTileIndex(x, y); }

	function SetRailType()
	{
		local l = GSRailTypeList();
		foreach (rt, _ in l) {
			if (GSRail.IsRailTypeAvailable(rt)) {
				GSRail.SetCurrentRailType(rt);
				return rt;
			}
		}
		return null;
	}

	/* dir names are the diagonal direction the depot entrance faces: NE x-1, SE y+1, SW x+1, NW y-1 */
	function DirOffset(dir)
	{
		switch (dir.toupper()) {
			case "NE": return [-1, 0];
			case "SE": return [0, 1];
			case "SW": return [1, 0];
			case "NW": return [0, -1];
		}
		return null;
	}

	function Sgn(a) { return a > 0 ? 1 : (a < 0 ? -1 : 0); }

	function StateName(s)
	{
		switch (s) {
			case GSVehicle.VS_RUNNING: return "running";
			case GSVehicle.VS_STOPPED: return "stopped";
			case GSVehicle.VS_IN_DEPOT: return "depot";
			case GSVehicle.VS_AT_STATION: return "station";
			case GSVehicle.VS_BROKEN: return "broken";
			case GSVehicle.VS_CRASHED: return "crashed";
		}
		return "invalid";
	}

	function Need(msg, keys, id)
	{
		foreach (k in keys) {
			if (!(k in msg)) { this.Err(id, "missing " + k); return false; }
		}
		return true;
	}

	/* ---- dispatch ---- */
	function Handle(msg)
	{
		if (typeof(msg) != "table" || !("cmd" in msg)) return;
		local id = ("id" in msg) ? msg.id : 0;
		local cmd = msg.cmd;
		if (cmd == "ping") {
			this.Reply(id, { pong = true, date = GSDate.GetCurrentDate() });
			return;
		}
		if (cmd == "company_set") {
			if (!this.Need(msg, ["company"], id)) return;
			if (GSCompany.ResolveCompanyID(msg.company) == GSCompany.COMPANY_INVALID) {
				this.Err(id, "no such company " + msg.company);
				return;
			}
			this.company = msg.company;
			this.Reply(id, { company = this.company });
			return;
		}
		if (cmd == "vehicles") {
			this.CmdVehicles(id);
			return;
		}
		/* everything below acts as the chosen company */
		if (this.company < 0) { this.Err(id, "no company set (company_set first)"); return; }
		local mode = GSCompanyMode(this.company);
		if (cmd == "build_rail") this.CmdBuildRail(id, msg);
		else if (cmd == "build_depot") this.CmdBuildDepot(id, msg);
		else if (cmd == "build_train") this.CmdBuildTrain(id, msg);
		else if (cmd == "orders_depots") this.CmdOrders(id, msg);
		else if (cmd == "start") this.CmdStart(id, msg);
		else this.Err(id, "unknown cmd");
	}

	function CmdBuildRail(id, msg)
	{
		if (!this.Need(msg, ["x1", "y1", "x2", "y2"], id)) return;
		local dx = this.Sgn(msg.x2 - msg.x1), dy = this.Sgn(msg.y2 - msg.y1);
		if (dx != 0 && dy != 0) { this.Err(id, "line must be straight"); return; }
		if (dx == 0 && dy == 0) { this.Err(id, "line needs at least two tiles"); return; }
		if (this.SetRailType() == null) { this.Err(id, "no rail type available"); return; }
		local from = this.Tile(msg.x1 - dx, msg.y1 - dy);
		local first = this.Tile(msg.x1, msg.y1);
		local to = this.Tile(msg.x2 + dx, msg.y2 + dy);
		if (!GSRail.BuildRail(from, first, to)) {
			this.Err(id, "BuildRail failed: " + GSError.GetLastErrorString());
			return;
		}
		this.Reply(id, { ok = true });
	}

	function CmdBuildDepot(id, msg)
	{
		if (!this.Need(msg, ["x", "y", "dir"], id)) return;
		local off = this.DirOffset(msg.dir);
		if (off == null) { this.Err(id, "dir must be NE, SE, SW or NW"); return; }
		if (this.SetRailType() == null) { this.Err(id, "no rail type available"); return; }
		local t = this.Tile(msg.x, msg.y);
		if (!GSRail.BuildRailDepot(t, this.Tile(msg.x + off[0], msg.y + off[1]))) {
			this.Err(id, "BuildRailDepot failed: " + GSError.GetLastErrorString());
			return;
		}
		this.Reply(id, { ok = true });
	}

	function CmdBuildTrain(id, msg)
	{
		if (!this.Need(msg, ["depot_x", "depot_y"], id)) return;
		local rt = this.SetRailType();
		if (rt == null) { this.Err(id, "no rail type available"); return; }
		local depot = this.Tile(msg.depot_x, msg.depot_y);
		local engine = ("engine" in msg) ? msg.engine : null;
		local wagon = null;
		local cargos = GSCargoList();
		local engines = GSEngineList(GSVehicle.VT_RAIL);
		local best_speed = -1;
		foreach (e, _ in engines) {
			if (!GSEngine.IsBuildable(e) || !GSEngine.CanRunOnRail(e, rt)) continue;
			if (GSEngine.IsWagon(e)) {
				if (wagon == null) {
					foreach (c, _ in cargos) {
						if (GSEngine.CanRefitCargo(e, c)) { wagon = e; break; }
					}
				}
			} else if (engine == null || !("engine" in msg)) {
				local sp = GSEngine.GetMaxSpeed(e);
				if (sp > best_speed) { best_speed = sp; engine = e; }
			}
		}
		if (engine == null) { this.Err(id, "no buildable engine"); return; }
		local n = ("wagons" in msg) ? msg.wagons : 0;
		if (n > 0 && wagon == null) { this.Err(id, "no buildable wagon"); return; }
		local v = GSVehicle.BuildVehicle(depot, engine);
		if (!GSVehicle.IsValidVehicle(v)) {
			this.Err(id, "BuildVehicle failed: " + GSError.GetLastErrorString());
			return;
		}
		local got = 0;
		for (local i = 0; i < n; i++) {
			local w = GSVehicle.BuildVehicle(depot, wagon);
			if (!GSVehicle.IsValidVehicle(w)) break;
			if (GSVehicle.MoveWagon(w, 0, v, GSVehicle.GetNumWagons(v) - 1)) got++;
		}
		this.Reply(id, { vehicle = v, engine = engine, engine_name = GSEngine.GetName(engine), wagons = got });
	}

	function CmdOrders(id, msg)
	{
		if (!this.Need(msg, ["vehicle", "x1", "y1", "x2", "y2"], id)) return;
		if (!GSVehicle.IsValidVehicle(msg.vehicle)) { this.Err(id, "bad vehicle"); return; }
		local ok1 = GSOrder.AppendOrder(msg.vehicle, this.Tile(msg.x1, msg.y1), GSOrder.OF_NON_STOP_INTERMEDIATE);
		local ok2 = GSOrder.AppendOrder(msg.vehicle, this.Tile(msg.x2, msg.y2), GSOrder.OF_NON_STOP_INTERMEDIATE);
		if (!ok1 || !ok2) {
			this.Err(id, "AppendOrder failed: " + GSError.GetLastErrorString());
			return;
		}
		this.Reply(id, { orders = GSOrder.GetOrderCount(msg.vehicle) });
	}

	function CmdStart(id, msg)
	{
		if (!this.Need(msg, ["vehicle"], id)) return;
		if (!GSVehicle.IsValidVehicle(msg.vehicle)) { this.Err(id, "bad vehicle"); return; }
		local st = GSVehicle.GetState(msg.vehicle);
		if (st != GSVehicle.VS_RUNNING) {
			if (!GSVehicle.StartStopVehicle(msg.vehicle)) {
				this.Err(id, "StartStopVehicle failed: " + GSError.GetLastErrorString());
				return;
			}
		}
		this.Reply(id, { started = true });
	}

	function CmdVehicles(id)
	{
		local out = [];
		foreach (v, _ in GSVehicleList()) {
			if (GSVehicle.GetVehicleType(v) != GSVehicle.VT_RAIL) continue;
			local t = GSVehicle.GetLocation(v);
			out.append({
				id = v,
				x = GSMap.GetTileX(t),
				y = GSMap.GetTileY(t),
				speed = GSVehicle.GetCurrentSpeed(v),
				state = this.StateName(GSVehicle.GetState(v))
			});
		}
		this.Reply(id, { date = GSDate.GetCurrentDate(), vehicles = out });
	}
}
