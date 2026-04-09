#!/usr/bin/env python3
"""
SteamForge 游戏下载服务端
支持: 游戏清单、断点续传、文件校验
"""

import os
import json
import hashlib
from pathlib import Path
from flask import Flask, request, Response, jsonify, send_file

app = Flask(__name__)

# 配置
DATA_DIR = Path(__file__).parent / "data"
MANIFESTS_DIR = DATA_DIR / "manifests"
FILES_DIR = DATA_DIR / "files"

# 确保目录存在
MANIFESTS_DIR.mkdir(parents=True, exist_ok=True)
FILES_DIR.mkdir(parents=True, exist_ok=True)


@app.route("/ping")
def ping():
    """健康检查"""
    return "pong"


@app.route("/api/manifest/<app_id>")
def get_manifest(app_id):
    """获取游戏清单"""
    manifest_path = MANIFESTS_DIR / f"{app_id}.json"
    if not manifest_path.exists():
        return jsonify({"error": "Game not found"}), 404

    with open(manifest_path, "r", encoding="utf-8") as f:
        return jsonify(json.load(f))


@app.route("/api/games")
def list_games():
    """列出所有可用游戏"""
    games = []
    for manifest_file in MANIFESTS_DIR.glob("*.json"):
        try:
            with open(manifest_file, "r", encoding="utf-8") as f:
                data = json.load(f)
                games.append({
                    "appId": data.get("appId"),
                    "name": data.get("name"),
                    "version": data.get("version"),
                    "totalSize": data.get("totalSize", 0),
                    "totalCompressed": data.get("totalCompressed", 0)
                })
        except Exception as e:
            print(f"Error reading {manifest_file}: {e}")

    return jsonify({"games": games})


@app.route("/files/<path:file_path>")
def download_file(file_path):
    """下载文件 (支持断点续传)"""
    full_path = FILES_DIR / file_path

    if not full_path.exists():
        return jsonify({"error": "File not found"}), 404

    file_size = full_path.stat().st_size

    # 处理Range请求 (断点续传)
    range_header = request.headers.get("Range")
    if range_header:
        # 解析 Range: bytes=start-end
        range_match = range_header.replace("bytes=", "").split("-")
        start = int(range_match[0]) if range_match[0] else 0
        end = int(range_match[1]) if range_match[1] else file_size - 1

        if start >= file_size:
            return Response(status=416)  # Range Not Satisfiable

        end = min(end, file_size - 1)
        length = end - start + 1

        with open(full_path, "rb") as f:
            f.seek(start)
            data = f.read(length)

        response = Response(
            data,
            status=206,  # Partial Content
            mimetype="application/octet-stream"
        )
        response.headers["Content-Range"] = f"bytes {start}-{end}/{file_size}"
        response.headers["Content-Length"] = length
        response.headers["Accept-Ranges"] = "bytes"
        return response

    # 完整文件下载
    return send_file(full_path, as_attachment=True)


def compute_sha256(file_path):
    """计算文件SHA256"""
    sha256 = hashlib.sha256()
    with open(file_path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            sha256.update(chunk)
    return sha256.hexdigest()


def generate_manifest(app_id, game_name, game_dir):
    """为游戏目录生成清单"""
    game_path = FILES_DIR / game_dir
    if not game_path.exists():
        print(f"Error: {game_path} does not exist")
        return None

    files = []
    total_size = 0

    for file_path in game_path.rglob("*"):
        if file_path.is_file():
            rel_path = file_path.relative_to(game_path)
            size = file_path.stat().st_size
            sha256 = compute_sha256(file_path)

            files.append({
                "path": str(rel_path).replace("\\", "/"),
                "size": size,
                "compressedSize": size,  # 未压缩时相同
                "sha256": sha256,
                "compressed": False
            })
            total_size += size
            print(f"  {rel_path}: {size} bytes")

    manifest = {
        "appId": app_id,
        "name": game_name,
        "version": "1.0.0",
        "totalSize": total_size,
        "totalCompressed": total_size,
        "files": files
    }

    manifest_path = MANIFESTS_DIR / f"{app_id}.json"
    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2, ensure_ascii=False)

    print(f"Manifest saved to {manifest_path}")
    return manifest


@app.route("/api/admin/generate-manifest", methods=["POST"])
def admin_generate_manifest():
    """管理接口: 生成游戏清单"""
    data = request.json
    app_id = data.get("appId")
    game_name = data.get("name")
    game_dir = data.get("dir", app_id)

    if not app_id or not game_name:
        return jsonify({"error": "Missing appId or name"}), 400

    manifest = generate_manifest(app_id, game_name, game_dir)
    if manifest:
        return jsonify(manifest)
    return jsonify({"error": "Failed to generate manifest"}), 500


if __name__ == "__main__":
    print("=" * 50)
    print("SteamForge Download Server")
    print("=" * 50)
    print(f"Data directory: {DATA_DIR}")
    print(f"Manifests: {MANIFESTS_DIR}")
    print(f"Files: {FILES_DIR}")
    print()
    print("API Endpoints:")
    print("  GET  /ping                    - Health check")
    print("  GET  /api/games               - List all games")
    print("  GET  /api/manifest/<appId>    - Get game manifest")
    print("  GET  /files/<path>            - Download file")
    print("  POST /api/admin/generate-manifest - Generate manifest")
    print()
    print("Starting server on http://0.0.0.0:8080")
    print("=" * 50)

    app.run(host="0.0.0.0", port=8080, debug=True)
