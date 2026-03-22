from flask import Flask, request, jsonify
import datetime

app = Flask(__name__)

# 模拟数据库，初始化数据
items = {"sensor": {"name": "Sensor", "Value": 0}}


# 根目录接口
@app.route("/", methods=["GET"])
def read_root():
    return {"Hello": "World"}


# 读取数据接口
# 例如访问: /items/sensor
@app.route("/items/<item_id>", methods=["GET"])
def read_items(item_id):
    if item_id in items:
        # 返回数据和当前时间
        return {
            "data": items[item_id],
            "timestamp": datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        }
    else:
        return {"error": "Item not found"}, 404


# 更新数据接口 (供 XIAO ESP32C3 调用)
@app.route("/sensor/", methods=["POST"])
def update_sensor():
    # 获取 ESP32C3 发送过来的 JSON 数据
    data = request.get_json()

    if not data or "value" not in data:
        return {"error": "Invalid data"}, 400

    # 更新内存中的数据
    # 注意：这里假设 items["sensor"] 已经存在
    if "sensor" in items:
        items["sensor"]["Value"] = data["value"]

    # 返回收到的数据，确认成功
    return jsonify(data), 200


if __name__ == "__main__":
    # host='0.0.0.0' 允许局域网内的 ESP32C3 访问
    app.run(host="0.0.0.0", port=8000, debug=True)
