"""
universal_game_server.py
Fully featured Universal Game/App Server
- Key-based auth
- Room-based multiplayer (WebSocket)
- HTTP API for room data
- Upload/download/delete files
- Search files
- Dynamic static serving
- Persistent room state
- Configurable via config.txt
- Auto-reloads config when changed
"""

import asyncio, json, logging, pathlib, random, string, os, time
from aiohttp import web
import aiohttp_cors
from datetime import datetime

# ---------------- Paths -----------------
BASE_DIR = pathlib.Path(__file__).parent
CONFIG_FILE = BASE_DIR / "config.txt"
SERVER_DATA = BASE_DIR / "server_data"
SERVER_DATA.mkdir(exist_ok=True)

# ---------------- Config -----------------
def generate_key(length=16):
    return "".join(random.choices(string.ascii_letters + string.digits, k=length))

# Create default config if missing
if not CONFIG_FILE.exists():
    print("Config not found, generating new config.txt...")
    with open(CONFIG_FILE, "w") as f:
        f.write("\n".join([
            "SERVER_IP=0.0.0.0",
            "SERVER_PORT=8080",
            f"AUTH_KEY={generate_key()}",
            "AUTO_OPEN_BROWSER=True",
            "LOG_LEVEL=INFO",
            "MAX_UPLOAD_SIZE_MB=50",
            "ROOM_PERSISTENCE=True",
            "ALLOW_DELETE=True",
            "STATIC_DIR=static",
            "CORS_ENABLED=True",
            "DEFAULT_ROOM=default",
            "FILE_SEARCH_PREFIX=",
            "MAINTENANCE_MODE=False"
        ]))

# ---------------- Config Reload -----------------
CONFIG = {}
AUTHORIZED_KEYS = set()

def load_config():
    CONFIG.clear()
    with open(CONFIG_FILE,"r") as f:
        for line in f:
            if "=" in line:
                k,v = line.strip().split("=",1)
                CONFIG[k.strip()] = v.strip()
    global SERVER_IP, PORT, AUTO_OPEN_BROWSER, LOG_LEVEL, MAX_UPLOAD_SIZE, ROOM_PERSISTENCE
    global ALLOW_DELETE, STATIC_DIR, CORS_ENABLED, DEFAULT_ROOM, FILE_SEARCH_PREFIX, MAINTENANCE_MODE
    SERVER_IP = CONFIG.get("SERVER_IP","0.0.0.0")
    PORT = int(CONFIG.get("SERVER_PORT",8080))
    AUTHORIZED_KEYS.clear()
    AUTHORIZED_KEYS.add(CONFIG.get("AUTH_KEY",generate_key()))
    AUTO_OPEN_BROWSER = CONFIG.get("AUTO_OPEN_BROWSER","True").lower()=="true"
    LOG_LEVEL = getattr(logging, CONFIG.get("LOG_LEVEL","INFO").upper(), logging.INFO)
    MAX_UPLOAD_SIZE = int(CONFIG.get("MAX_UPLOAD_SIZE_MB",50))*1024*1024
    ROOM_PERSISTENCE = CONFIG.get("ROOM_PERSISTENCE","True").lower()=="true"
    ALLOW_DELETE = CONFIG.get("ALLOW_DELETE","True").lower()=="true"
    STATIC_DIR = BASE_DIR / CONFIG.get("STATIC_DIR","static")
    STATIC_DIR.mkdir(exist_ok=True)
    CORS_ENABLED = CONFIG.get("CORS_ENABLED","True").lower()=="true"
    DEFAULT_ROOM = CONFIG.get("DEFAULT_ROOM","default")
    FILE_SEARCH_PREFIX = CONFIG.get("FILE_SEARCH_PREFIX","")
    MAINTENANCE_MODE = CONFIG.get("MAINTENANCE_MODE","False").lower()=="true"

load_config()
CONFIG_LAST_MODIFIED = CONFIG_FILE.stat().st_mtime

def reload_config():
    global CONFIG_LAST_MODIFIED
    try:
        mtime = CONFIG_FILE.stat().st_mtime
        if mtime != CONFIG_LAST_MODIFIED:
            print("Reloading config.txt...")
            load_config()
            CONFIG_LAST_MODIFIED = mtime
    except Exception as e:
        print("Failed to reload config:", e)

# ---------------- Logging -----------------
log_file = BASE_DIR / "server.log"
logging.basicConfig(
    level=LOG_LEVEL,
    format="[%(asctime)s] %(message)s",
    handlers=[logging.FileHandler(log_file, encoding="utf-8"), logging.StreamHandler()]
)
LOG = logging.getLogger("UniversalGameServer")

# ---------------- Helpers -----------------
ROOMS = {}      # room_name -> set of websockets
ROOM_DATA = {}  # room_name -> persistent dict

def iso_now():
    return datetime.utcnow().isoformat()+"Z"

def safe_name(name: str):
    return "".join(ch for ch in name if ch.isalnum() or ch in "-_").strip() or DEFAULT_ROOM

async def broadcast(room: str, message: dict, exclude=None):
    if room not in ROOMS:
        return
    text = json.dumps(message)
    tasks = [ws.send_str(text) for ws in list(ROOMS[room]) if ws is not exclude and not ws.closed]
    if tasks:
        await asyncio.gather(*tasks, return_exceptions=True)

def save_room_data(room):
    if not ROOM_PERSISTENCE: return
    path = SERVER_DATA / f"{room}.json"
    with open(path,"w",encoding="utf-8") as f:
        json.dump(ROOM_DATA.get(room,{}), f, indent=2)

def load_room_data(room):
    path = SERVER_DATA / f"{room}.json"
    if path.exists():
        ROOM_DATA[room] = json.load(open(path,"r",encoding="utf-8"))
    else:
        ROOM_DATA[room] = {}

# ---------------- Middleware -----------------
@web.middleware
async def auth_middleware(request, handler):
    if MAINTENANCE_MODE:
        return web.json_response({"ok":False,"error":"Server in maintenance"}, status=503)
    key = request.query.get("key") or request.headers.get("X-API-Key")
    if key not in AUTHORIZED_KEYS:
        return web.json_response({"ok": False, "error": "unauthorized"}, status=401)
    return await handler(request)

# ---------------- Routes -----------------
async def index(request):
    rooms = [p.stem for p in SERVER_DATA.glob("*.json")]
    files = [p.name for p in SERVER_DATA.glob("*") if not p.suffix==".json"]
    return web.json_response({"ok":True,"rooms":rooms,"files":files})

# --- WebSocket ---
async def websocket_handler(request):
    key = request.query.get("key")
    if key not in AUTHORIZED_KEYS:
        return web.Response(status=401,text="Invalid key")

    ws = web.WebSocketResponse()
    await ws.prepare(request)

    peer = request.remote
    client_id = f"{peer}-{iso_now()}"
    room = DEFAULT_ROOM

    LOG.info("WebSocket connected: %s",peer)
    await ws.send_json({"type":"welcome","time":iso_now()})

    try:
        async for msg in ws:
            if msg.type == web.WSMsgType.TEXT:
                try: data = json.loads(msg.data)
                except Exception:
                    await ws.send_json({"error":"invalid json"})
                    continue

                typ = data.get("type")
                if typ=="join":
                    room = safe_name(data.get("room",DEFAULT_ROOM))
                    ROOMS.setdefault(room,set()).add(ws)
                    load_room_data(room)
                    await ws.send_json({"type":"joined","room":room})
                    await broadcast(room, {"type":"notice","msg":f"{client_id} joined"}, exclude=ws)
                elif typ=="msg":
                    await broadcast(room, {"type":"msg","from":client_id,"text":data.get("text","")})
                elif typ=="state":
                    ROOM_DATA.setdefault(room,{})
                    ROOM_DATA[room].update(data.get("payload",{}))
                    save_room_data(room)
                    await broadcast(room, {"type":"state","from":client_id,"payload":data.get("payload")})
            elif msg.type==web.WSMsgType.ERROR:
                LOG.error("WebSocket error: %s", ws.exception())
    finally:
        if room in ROOMS and ws in ROOMS[room]:
            ROOMS[room].remove(ws)
        LOG.info("WebSocket disconnected: %s",peer)

    return ws

# --- HTTP API ---
async def api_room_get(request):
    room = safe_name(request.match_info["room"])
    load_room_data(room)
    return web.json_response({"ok":True,"room":room,"data":ROOM_DATA.get(room,{})})

async def api_room_post(request):
    room = safe_name(request.match_info["room"])
    data = await request.json()
    ROOM_DATA.setdefault(room,{})
    ROOM_DATA[room].update(data)
    save_room_data(room)
    return web.json_response({"ok":True,"room":room,"data":ROOM_DATA[room]})

# --- File endpoints ---
async def serve_file(request):
    file_path = request.match_info["file_path"]
    full_path = SERVER_DATA / file_path
    if full_path.exists() and full_path.is_file():
        return web.FileResponse(full_path)
    return web.Response(status=404,text="File not found")

async def upload_file(request):
    reader = await request.multipart()
    field = await reader.next()
    if field is None or field.name!="file":
        return web.json_response({"ok":False,"error":"no file uploaded"},status=400)
    filename = field.filename
    save_path = SERVER_DATA / filename
    total = 0
    with open(save_path,"wb") as f:
        while True:
            chunk = await field.read_chunk()
            if not chunk: break
            total += len(chunk)
            if total>MAX_UPLOAD_SIZE:
                return web.json_response({"ok":False,"error":"File too large"},status=413)
            f.write(chunk)
    LOG.info("File uploaded: %s",filename)
    return web.json_response({"ok":True,"filename":filename})

async def delete_file(request):
    if not ALLOW_DELETE:
        return web.json_response({"ok":False,"error":"Deletion disabled"},status=403)
    filename = request.match_info["filename"]
    path = SERVER_DATA / filename
    if path.exists() and path.is_file():
        path.unlink()
        LOG.info("File deleted: %s",filename)
        return web.json_response({"ok":True,"filename":filename})
    return web.json_response({"ok":False,"error":"file not found"},status=404)

async def list_files(request):
    prefix = request.query.get("prefix",FILE_SEARCH_PREFIX)
    files = [p.name for p in SERVER_DATA.glob(f"{prefix}*") if p.is_file()]
    return web.json_response({"ok":True,"files":files})

# --- Delete room ---
async def delete_room(request):
    if not ALLOW_DELETE:
        return web.json_response({"ok":False,"error":"Deletion disabled"},status=403)
    room = safe_name(request.match_info["room"])
    path = SERVER_DATA / f"{room}.json"
    if path.exists():
        path.unlink()
        ROOM_DATA.pop(room,None)
        LOG.info("Room deleted: %s",room)
        return web.json_response({"ok":True,"room":room})
    return web.json_response({"ok":False,"error":"room not found"},status=404)

# ---------------- Server -----------------
def create_app():
    app = web.Application(middlewares=[auth_middleware])

    # Routes
    app.router.add_get("/",index)
    app.router.add_get("/ws",websocket_handler)
    app.router.add_get("/room/{room}",api_room_get)
    app.router.add_post("/room/{room}",api_room_post)
    app.router.add_post("/file/{filename}",upload_file)
    app.router.add_delete("/file/{filename}",delete_file)
    app.router.add_get("/file/{file_path:.*}",serve_file)
    app.router.add_get("/files",list_files)
    app.router.add_delete("/room/{room}",delete_room)

    # CORS
    if CORS_ENABLED:
        cors = aiohttp_cors.setup(app, defaults={
            "*": aiohttp_cors.ResourceOptions(
                allow_credentials=True, expose_headers="*", allow_headers="*"
            )
        })
        for route in list(app.router.routes()):
            cors.add(route)

    return app

# ---------------- Main -----------------
if __name__=="__main__":
    import webbrowser
    runner = web.AppRunner(create_app())

    async def run():
        await runner.setup()
        site = web.TCPSite(runner, SERVER_IP, PORT)
        await site.start()
        LOG.info("Server started at http://%s:%d", SERVER_IP, PORT)
        LOG.info("Authorized key: %s",list(AUTHORIZED_KEYS)[0])
        if AUTO_OPEN_BROWSER:
            webbrowser.open(f"http://localhost:{PORT}/?key={list(AUTHORIZED_KEYS)[0]}")

        CHECK_INTERVAL = 5
        while True:
            reload_config()  # automatically reload config changes
            await asyncio.sleep(CHECK_INTERVAL)

    try:
        asyncio.run(run())
    except KeyboardInterrupt:
        LOG.info("Server stopped")
