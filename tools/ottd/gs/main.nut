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

	/* names of the buildable rail engines (wagons if wagons is true), for error messages */
	function RailNames(wagons)
	{
		local out = "";
		foreach (e, _ in GSEngineList(GSVehicle.VT_RAIL)) {
			if (GSEngine.IsBuildable(e) && GSEngine.IsWagon(e) == wagons) out += GSEngine.GetName(e) + "; ";
		}
		return out;
	}

	/* with an engine, only a rail type it can run on */
	function SetRailType(engine = null)
	{
		local l = GSRailTypeList();
		foreach (rt, _ in l) {
			if (engine != null && !GSEngine.CanRunOnRail(engine, rt)) continue;
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
		else if (cmd == "build_waypoint") this.CmdBuildWaypoint(id, msg);
		else if (cmd == "orders_via") this.CmdOrdersVia(id, msg);
		else if (cmd == "start") this.CmdStart(id, msg);
		else if (cmd == "build_layout") this.CmdBuildLayout(id, msg);
		else if (cmd == "sample") this.CmdSample(id, msg);
		else if (cmd == "speed_trace") this.CmdSpeedTrace(id, msg);
		else if (cmd == "occupancy") this.CmdOccupancy(id, msg);
		else if (cmd == "loan_max") this.CmdLoanMax(id);
		else if (cmd == "find_clear") this.CmdFindClear(id, msg);
		else if (cmd == "raise_rect") this.CmdRaiseRect(id, msg);
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

	function CmdLoanMax(id)
	{
		if (!GSCompany.SetLoanAmount(GSCompany.GetMaxLoanAmount())) {
			this.Err(id, "SetLoanAmount failed: " + GSError.GetLastErrorString());
			return;
		}
		this.Reply(id, { balance = GSCompany.GetBankBalance(this.company) });
	}

	function CmdBuildTrain(id, msg)
	{
		if (!this.Need(msg, ["depot_x", "depot_y"], id)) return;
		local engine_name = ("engine_name" in msg) ? msg.engine_name : null;
		local wagon_name = ("wagon_name" in msg) ? msg.wagon_name : null;
		local rt = null;
		if (engine_name != null) {
			/* the rail type is one the named, buildable engine can run on */
			local named = null;
			foreach (e, _ in GSEngineList(GSVehicle.VT_RAIL)) {
				if (!GSEngine.IsWagon(e) && GSEngine.IsBuildable(e) && GSEngine.GetName(e) == engine_name) { named = e; break; }
			}
			if (named == null) { this.Err(id, "no buildable engine named " + engine_name + ": " + this.RailNames(false)); return; }
			rt = this.SetRailType(named);
			if (rt == null) { this.Err(id, "no available rail type for engine " + engine_name); return; }
		} else {
			rt = this.SetRailType();
			if (rt == null) { this.Err(id, "no rail type available"); return; }
		}
		local depot = this.Tile(msg.depot_x, msg.depot_y);
		local engine = ("engine" in msg) ? msg.engine : null;
		local wagon = null;
		local cargos = GSCargoList();
		local engines = GSEngineList(GSVehicle.VT_RAIL);
		local best_speed = -1;
		foreach (e, _ in engines) {
			if (!GSEngine.IsBuildable(e) || !GSEngine.CanRunOnRail(e, rt)) continue;
			if (GSEngine.IsWagon(e)) {
				if (wagon_name != null) {
					if (wagon == null && GSEngine.GetName(e) == wagon_name) wagon = e;
				} else if (wagon == null) {
					foreach (c, _ in cargos) {
						if (GSEngine.CanRefitCargo(e, c)) { wagon = e; break; }
					}
				}
			} else if (engine_name != null) {
				if (engine == null && GSEngine.GetName(e) == engine_name) engine = e;
			} else if (engine == null || !("engine" in msg)) {
				local sp = GSEngine.GetMaxSpeed(e);
				if (sp > best_speed) { best_speed = sp; engine = e; }
			}
		}
		if (engine == null) { this.Err(id, "no buildable engine"); return; }
		local n = ("wagons" in msg) ? msg.wagons : 0;
		if (n > 0 && wagon == null) { this.Err(id, "no buildable wagon" + (wagon_name != null ? " named " + wagon_name + ": " + this.RailNames(true) : "")); return; }
		local v = GSVehicle.BuildVehicle(depot, engine);
		if (!GSVehicle.IsValidVehicle(v)) {
			this.Err(id, "BuildVehicle failed: " + GSError.GetLastErrorString());
			return;
		}
		local nengines = ("engines" in msg) ? msg.engines : 1;
		local extra = 1;
		for (local i = 1; i < nengines; i++) {
			local e2 = GSVehicle.BuildVehicle(depot, engine);
			if (!GSVehicle.IsValidVehicle(e2)) break;
			if (GSVehicle.MoveWagon(e2, 0, v, 0)) extra++;
		}
		local got = 0;
		for (local i = 0; i < n; i++) {
			local w = GSVehicle.BuildVehicle(depot, wagon);
			if (!GSVehicle.IsValidVehicle(w)) break;
			if (GSVehicle.MoveWagon(w, 0, v, GSVehicle.GetNumWagons(v) - 1)) got++;
		}
		this.Reply(id, { vehicle = v, engine = engine, engine_name = GSEngine.GetName(engine), engines = extra, wagons = got });
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

	function CmdBuildWaypoint(id, msg)
	{
		if (!this.Need(msg, ["x", "y"], id)) return;
		if (this.SetRailType() == null) { this.Err(id, "no rail type available"); return; }
		if (!GSRail.BuildRailWaypoint(this.Tile(msg.x, msg.y))) {
			this.Err(id, "BuildRailWaypoint failed: " + GSError.GetLastErrorString());
			return;
		}
		this.Reply(id, { ok = true });
	}

	function CmdOrdersVia(id, msg)
	{
		if (!this.Need(msg, ["vehicle", "tiles"], id)) return;
		if (!GSVehicle.IsValidVehicle(msg.vehicle)) { this.Err(id, "bad vehicle"); return; }
		foreach (t in msg.tiles) {
			if (!GSOrder.AppendOrder(msg.vehicle, this.Tile(t[0], t[1]), GSOrder.OF_NON_STOP_INTERMEDIATE)) {
				this.Err(id, "AppendOrder failed: " + GSError.GetLastErrorString());
				return;
			}
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

	/* ---- layouts ---- */
	/* Track names -> ScriptRail.RailTrack. */
	function TrackByName(n)
	{
		switch (n) {
			case "X": return GSRail.RAILTRACK_NE_SW;
			case "Y": return GSRail.RAILTRACK_NW_SE;
			case "Upper": return GSRail.RAILTRACK_NW_NE;
			case "Lower": return GSRail.RAILTRACK_SW_SE;
			case "Left": return GSRail.RAILTRACK_NW_SW;
			case "Right": return GSRail.RAILTRACK_NE_SE;
		}
		return null;
	}

	/* Copy of _possible_trackdirs in src/script/api/script_rail.cpp, flattened: facing trackdir ->
	 * [front dx, front dy, track]. The front tile is the neighbour the train comes from, i.e. the side the signal is seen from. */
	function FacingMap(f)
	{
		switch (f) {
			case "Upper_E": return [0, -1, GSRail.RAILTRACK_NW_NE];
			case "Y_SE":    return [0, -1, GSRail.RAILTRACK_NW_SE];
			case "Left_S":  return [0, -1, GSRail.RAILTRACK_NW_SW];
			case "Right_S": return [-1, 0, GSRail.RAILTRACK_NE_SE];
			case "X_SW":    return [-1, 0, GSRail.RAILTRACK_NE_SW];
			case "Upper_W": return [-1, 0, GSRail.RAILTRACK_NW_NE];
			case "Lower_E": return [1, 0, GSRail.RAILTRACK_SW_SE];
			case "X_NE":    return [1, 0, GSRail.RAILTRACK_NE_SW];
			case "Left_N":  return [1, 0, GSRail.RAILTRACK_NW_SW];
			case "Right_N": return [0, 1, GSRail.RAILTRACK_NE_SE];
			case "Y_NW":    return [0, 1, GSRail.RAILTRACK_NW_SE];
			case "Lower_W": return [0, 1, GSRail.RAILTRACK_SW_SE];
		}
		return null;
	}

	/* Order in which BuildSignal tries tracks for a given front offset (row of _possible_trackdirs). */
	function FrontRow(dx, dy)
	{
		if (dy == -1) return [GSRail.RAILTRACK_NW_NE, GSRail.RAILTRACK_NW_SE, GSRail.RAILTRACK_NW_SW];
		if (dx == -1) return [GSRail.RAILTRACK_NE_SE, GSRail.RAILTRACK_NE_SW, GSRail.RAILTRACK_NW_NE];
		if (dx == 1) return [GSRail.RAILTRACK_SW_SE, GSRail.RAILTRACK_NE_SW, GSRail.RAILTRACK_NW_SW];
		return [GSRail.RAILTRACK_NE_SE, GSRail.RAILTRACK_NW_SE, GSRail.RAILTRACK_SW_SE];
	}

	function SignalByName(n)
	{
		switch (n) {
			case "block": return GSRail.SIGNALTYPE_NORMAL;
			case "entry": return GSRail.SIGNALTYPE_ENTRY;
			case "exit": return GSRail.SIGNALTYPE_EXIT;
			case "combo": return GSRail.SIGNALTYPE_COMBO;
			case "path": return GSRail.SIGNALTYPE_PBS;
			case "path_oneway": return GSRail.SIGNALTYPE_PBS_ONEWAY;
		}
		return null;
	}

	function FastestRailBridge(len)
	{
		local best = null, best_speed = -1;
		foreach (b, _ in GSBridgeList_Length(len)) {
			local sp = GSBridge.GetMaxSpeed(b);
			if (sp > best_speed) { best_speed = sp; best = b; }
		}
		return best;
	}

	function CmdBuildLayout(id, msg)
	{
		if (!this.Need(msg, ["origin_x", "origin_y", "layout"], id)) return;
		local ox = msg.origin_x, oy = msg.origin_y, lay = msg.layout;
		if ("railtype" in msg) {
			/* a rail type by name, e.g. "Electrified railway" */
			local found = false, names = "";
			foreach (rt, _ in GSRailTypeList()) {
				names += GSRail.GetName(rt) + "; ";
				if (GSRail.IsRailTypeAvailable(rt) && GSRail.GetName(rt) == msg.railtype) { GSRail.SetCurrentRailType(rt); found = true; break; }
			}
			if (!found) { this.Err(id, "no available rail type named " + msg.railtype + ": " + names); return; }
		} else if (this.SetRailType() == null) { this.Err(id, "no rail type available"); return; }
		local built = 0, failed = [];
		if ("stations" in lay) {
			foreach (st in lay.stations) {
				local dir = (st.axis == "X") ? GSRail.RAILTRACK_NE_SW : GSRail.RAILTRACK_NW_SE;
				if (GSRail.BuildRailStation(this.Tile(ox + st.x, oy + st.y), dir, st.platforms, st.length, GSStation.STATION_NEW)) built++;
				else failed.append({ what = "station", x = st.x, y = st.y, error = GSError.GetLastErrorString() });
			}
		}
		if ("tracks" in lay) {
			foreach (t in lay.tracks) {
				local in_station = false;
				local blocks = ("stations" in lay) ? lay.stations : (("stations_skip" in lay) ? lay.stations_skip : []);
				if (blocks.len() > 0) {
					foreach (st in blocks) {
						local w = (st.axis == "X") ? st.length : st.platforms;
						local h = (st.axis == "X") ? st.platforms : st.length;
						if (t.x >= st.x && t.x < st.x + w && t.y >= st.y && t.y < st.y + h) { in_station = true; break; }
					}
				}
				if (in_station) continue;
				foreach (name in t.bits) {
					local rt = this.TrackByName(name);
					if (rt == null) { failed.append({ what = "track " + name, x = t.x, y = t.y, error = "unknown track name" }); continue; }
					if (GSRail.BuildRailTrack(this.Tile(ox + t.x, oy + t.y), rt)) built++;
					else failed.append({ what = "track " + name, x = t.x, y = t.y, error = GSError.GetLastErrorString() });
				}
			}
		}
		if ("bridges" in lay) {
			foreach (b in lay.bridges) {
				local sx = b.start[0], sy = b.start[1], ex = b.end[0], ey = b.end[1];
				if ("tunnel" in b && b.tunnel) {
					if (GSTunnel.BuildTunnel(GSVehicle.VT_RAIL, this.Tile(ox + sx, oy + sy))) built++;
					else failed.append({ what = "tunnel", x = sx, y = sy, error = GSError.GetLastErrorString() });
					continue;
				}
				local len = abs(ex - sx) + abs(ey - sy) + 1;
				local bt = this.FastestRailBridge(len);
				if (bt == null) { failed.append({ what = "bridge", x = sx, y = sy, error = "no rail bridge type for length " + len }); continue; }
				if (GSBridge.BuildBridge(GSVehicle.VT_RAIL, bt, this.Tile(ox + sx, oy + sy), this.Tile(ox + ex, oy + ey))) built++;
				else failed.append({ what = "bridge", x = sx, y = sy, error = GSError.GetLastErrorString() });
			}
		}
		if ("signals" in lay) {
			foreach (s in lay.signals) {
				local what = "signal " + s.facing + " " + s.type;
				local fm = this.FacingMap(s.facing);
				local st = this.SignalByName(s.type);
				if (fm == null) { failed.append({ what = what, x = s.x, y = s.y, error = "unknown facing" }); continue; }
				if (st == null) { failed.append({ what = what, x = s.x, y = s.y, error = "unknown signal type" }); continue; }
				local tile = this.Tile(ox + s.x, oy + s.y);
				local front = this.Tile(ox + s.x + fm[0], oy + s.y + fm[1]);
				/* BuildSignal uses the first track of the front row that is on the tile; refuse if that is not ours */
				local first = null;
				foreach (rt in this.FrontRow(fm[0], fm[1])) {
					if (GSRail.IsRailTile(tile) && (GSRail.GetRailTracks(tile) & rt) != 0) { first = rt; break; }
				}
				if (first != fm[2]) { failed.append({ what = what, x = s.x, y = s.y, error = "no such track on tile, or front tile ambiguous" }); continue; }
				if (!GSRail.BuildSignal(tile, front, st)) {
					failed.append({ what = what, x = s.x, y = s.y, error = GSError.GetLastErrorString() });
					continue;
				}
				/* the engine must now show exactly a one-way signal on that facing */
				if (GSRail.GetSignalType(tile, front) != st) {
					failed.append({ what = what, x = s.x, y = s.y, error = "built signal reads back as type " + GSRail.GetSignalType(tile, front) });
					continue;
				}
				built++;
			}
		}
		this.Reply(id, { built = built, failed = failed });
	}

	/* ---- clear-space search ---- */
	/* Full check: buildable rectangle, every tile flat at the same height. */
	function RectClear(x, y, w, h)
	{
		if (!GSTile.IsBuildableRectangle(this.Tile(x, y), w, h)) return false;
		local hgt = GSTile.GetMinHeight(this.Tile(x, y));
		for (local j = 0; j < h; j++) {
			for (local i = 0; i < w; i++) {
				local t = this.Tile(x + i, y + j);
				if (GSTile.GetSlope(t) != GSTile.SLOPE_FLAT || GSTile.GetMinHeight(t) != hgt) return false;
			}
		}
		return true;
	}

	/* First origin (row-major from 2,2) with a clear w*h rectangle. Candidates are screened by their four
	 * corners being flat, then fully verified; the scan sleeps a tick when the ops budget runs low. */
	/* raise the tiles x0..x1, y0..y1 one level: the first tile's corners up, then the rectangle levelled to it; the tiles around become slopes */
	function CmdRaiseRect(id, msg)
	{
		if (!this.Need(msg, ["x0", "y0", "x1", "y1"], id)) return;
		local t0 = this.Tile(msg.x0, msg.y0);
		if (!GSTile.RaiseTile(t0, GSTile.SLOPE_ELEVATED)) { this.Err(id, "raise: " + GSError.GetLastErrorString()); return; }
		if (!GSTile.LevelTiles(t0, this.Tile(msg.x1, msg.y1))) { this.Err(id, "level: " + GSError.GetLastErrorString()); return; }
		this.Reply(id, { ok = true });
	}

	function CmdFindClear(id, msg)
	{
		if (!this.Need(msg, ["w", "h"], id)) return;
		local w = msg.w, h = msg.h;
		local mx = GSMap.GetMapSizeX(), my = GSMap.GetMapSizeY();
		for (local y = 2; y + h <= my - 1; y++) {
			for (local x = 2; x + w <= mx - 1; x++) {
				local ok = true;
				foreach (c in [[0, 0], [w - 1, 0], [0, h - 1], [w - 1, h - 1]]) {
					if (GSTile.GetSlope(this.Tile(x + c[0], y + c[1])) != GSTile.SLOPE_FLAT) { ok = false; break; }
				}
				if (ok && this.RectClear(x, y, w, h)) {
					this.Reply(id, { x = x, y = y });
					return;
				}
				if (GSController.GetOpsTillSuspend() < 5000) GSController.Sleep(1);
			}
		}
		this.Err(id, "no clear " + w + "x" + h + " area");
	}

	/* ---- speed sampling ---- */
	function CmdSample(id, msg)
	{
		if (!this.Need(msg, ["vehicles", "ticks"], id)) return;
		foreach (v in msg.vehicles) {
			if (!GSVehicle.IsValidVehicle(v)) { this.Err(id, "bad vehicle " + v); return; }
		}
		local n = msg.vehicles.len();
		local st = [];
		foreach (v in msg.vehicles) {
			st.append({ v = v, min = 1 << 30, max = 0, sum = 0, n = 0, stops = 0, prev = -1, hist = [], drops = [], cur = null });
		}
		for (local i = 0; i < msg.ticks; i++) {
			foreach (s in st) {
				local sp = GSVehicle.GetCurrentSpeed(s.v);
				local t = GSVehicle.GetLocation(s.v);
				if (sp < s.min) s.min = sp;
				if (sp > s.max) s.max = sp;
				s.sum += sp;
				s.n++;
				if (sp == 0 && s.prev > 0) s.stops++;
				/* drop: speed is 10 or more below the highest speed of the previous 8 ticks; consecutive ticks merge into one event */
				local hi = 0;
				foreach (h in s.hist) if (h > hi) hi = h;
				if (s.hist.len() > 0 && hi - sp >= 10) {
					if (s.cur == null) {
						s.cur = { tile_x = GSMap.GetTileX(t), tile_y = GSMap.GetTileY(t), from = hi, to = sp };
						s.drops.append(s.cur);
					} else {
						if (hi > s.cur.from) s.cur.from = hi;
						if (sp < s.cur.to) s.cur.to = sp;
					}
				} else s.cur = null;
				s.hist.append(sp);
				if (s.hist.len() > 8) s.hist.remove(0);
				s.prev = sp;
			}
			GSController.Sleep(1);
		}
		local out = [];
		foreach (s in st) {
			out.append({ id = s.v, n = s.n, min = s.n > 0 ? s.min : 0, max = s.max, avg = s.n > 0 ? s.sum / s.n : 0, stops = s.stops, drops = s.drops });
		}
		this.Reply(id, { vehicles = out });
	}

	/* speed_trace {vehicle,ticks}: GetCurrentSpeed (km/h) at every game tick, starting the tick the command arrives */
	function CmdSpeedTrace(id, msg)
	{
		if (!this.Need(msg, ["vehicle", "ticks"], id)) return;
		if (!GSVehicle.IsValidVehicle(msg.vehicle)) { this.Err(id, "bad vehicle " + msg.vehicle); return; }
		local speeds = [];
		for (local i = 0; i < msg.ticks; i++) {
			speeds.append(GSVehicle.GetCurrentSpeed(msg.vehicle));
			GSController.Sleep(1);
		}
		this.Reply(id, { speeds = speeds });
	}

	/* ---- waiting heatmap: stopped and slow samples per tile ---- */
	function CmdOccupancy(id, msg)
	{
		if (!this.Need(msg, ["vehicles", "ticks"], id)) return;
		foreach (v in msg.vehicles) {
			if (!GSVehicle.IsValidVehicle(v)) { this.Err(id, "bad vehicle " + v); return; }
		}
		local every = ("every" in msg) ? msg.every : 4;
		local slow_limit = ("slow" in msg) ? msg.slow : 80;
		local stopped = {};
		local slow = {};
		local prev = {};
		local stops = {};
		local box = ("box" in msg) ? msg.box : null;
		local in_box = {};
		foreach (v in msg.vehicles) { prev[v] <- -1; stops[v] <- 0; in_box[v] <- 0; }
		local samples = 0;
		for (local i = 0; i < msg.ticks; i += every) {
			foreach (v in msg.vehicles) {
				local sp = GSVehicle.GetCurrentSpeed(v);
				local t = GSVehicle.GetLocation(v);
				if (sp == 0) {
					stopped[t] <- (t in stopped) ? stopped[t] + 1 : 1;
					if (prev[v] > 0) stops[v]++;
					if (box != null) {
						local tx = GSMap.GetTileX(t), ty = GSMap.GetTileY(t);
						if (tx >= box[0] && tx < box[0] + box[2] && ty >= box[1] && ty < box[1] + box[3]) in_box[v]++;
					}
				} else if (sp < slow_limit) {
					slow[t] <- (t in slow) ? slow[t] + 1 : 1;
				}
				prev[v] = sp;
			}
			samples++;
			GSController.Sleep(every);
		}
		local to_list = function(tbl) {
			local l = [];
			foreach (t, n in tbl) l.append([GSMap.GetTileX(t), GSMap.GetTileY(t), n]);
			return l;
		};
		local sl = [];
		foreach (v in msg.vehicles) sl.append(box != null ? [v, stops[v], in_box[v]] : [v, stops[v]]);
		this.Reply(id, { samples = samples, stopped = to_list(stopped), slow = to_list(slow), stops = sl });
	}
}
