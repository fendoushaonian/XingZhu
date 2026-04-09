# SteamForge 游戏下载服务端

## API 接口

### 1. 健康检查
```
GET /ping
Response: "pong"
```

### 2. 获取游戏清单
```
GET /api/manifest/{appId}
Response:
{
  "appId": "730",
  "name": "Counter-Strike 2",
  "version": "1.0.0",
  "totalSize": 35000000000,
  "totalCompressed": 25000000000,
  "files": [
    {
      "path": "game/bin/cs2.exe",
      "size": 50000000,
      "compressedSize": 30000000,
      "sha256": "abc123...",
      "compressed": true
    }
  ]
}
```

### 3. 下载文件
```
GET /files/{path}
Headers: Range: bytes=0-1048575 (支持断点续传)
Response: 文件内容
```

### 4. 游戏列表
```
GET /api/games
Response:
{
  "games": [
    {"appId": "730", "name": "CS2", "size": 35000000000},
    {"appId": "570", "name": "Dota 2", "size": 40000000000}
  ]
}
```

## 目录结构
```
server/
├── data/
│   ├── manifests/     # 游戏清单JSON
│   │   ├── 730.json
│   │   └── 570.json
│   └── files/         # 游戏文件
│       ├── 730/
│       │   └── game/bin/cs2.exe
│       └── 570/
│           └── game/bin/dota2.exe
├── server.py          # Python服务端
└── README.md
```

## 快速启动
```bash
cd server
pip install flask
python server.py
```

服务端默认运行在 http://localhost:8080
