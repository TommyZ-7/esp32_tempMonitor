#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <deque>

const char* ssid = "xxx";
const char* password = "xxx";

// --- HTMLコンテンツをPROGMEMに格納 ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ja">
<head>
  <meta charset="utf-8">
  <title>Temp Monitor</title>
  <script src="https://cdn.tailwindcss.com"></script>
  <script src="https://unpkg.com/@babel/standalone@7.24.7/babel.min.js"></script>

  <script type="importmap">
    {
      "imports": {
        "react": "https://esm.sh/react@18.3.1",
        "react-dom/client": "https://esm.sh/react-dom@18.3.1/client",
        "recharts": "https://esm.sh/recharts@2.12.7",
        "framer-motion": "https://esm.sh/framer-motion@11.2.12",
        "lucide-react": "https://esm.sh/lucide-react@0.396.0"
      }
    }
  </script>

  <script type="text/babel" data-type="module">
    import React, { useState, useEffect, useRef } from "react";
    import { createRoot } from "react-dom/client";
    import {
      AreaChart,
      Area,
      XAxis,
      YAxis,
      CartesianGrid,
      Tooltip,
      ResponsiveContainer,
    } from "recharts";
    import { motion, AnimatePresence } from "framer-motion";
    import { Thermometer, History, Server, Zap } from "lucide-react";

    // Tooltipのカスタムコンポーネント
    const CustomTooltip = ({ active, payload, label }) => {
      if (active && payload && payload.length) {
        const secondsAgo = label;
        let timeLabel;

        if (secondsAgo === 0) {
          timeLabel = "現在";
        } else {
          const minutes = Math.floor(secondsAgo / 60);
          const seconds = secondsAgo % 60;
          if (minutes > 0) {
            timeLabel = `${minutes}分${seconds}秒前`;
          } else {
            timeLabel = `${seconds}秒前`;
          }
        }

        return (
          <div className="p-3 bg-gray-700/50 backdrop-blur-sm border border-gray-600 rounded-lg shadow-lg">
            <p className="text-sm text-cyan-300">{`温度: ${payload[0].value.toFixed(2)} °C`}</p>
            <p className="text-xs text-gray-400">{timeLabel}</p>
          </div>
        );
      }
      return null;
    };

    // 1桁の数字をスロットマシンのように表示するコンポーネント
    const DigitSlot = ({ digit }) => {
      const numbers = "0123456789";
      const digitIndex = numbers.indexOf(digit);
      const y = `-${digitIndex}em`;

      return (
        <motion.div
          animate={{ y }}
          transition={{ type: "spring", stiffness: 150, damping: 20 }}
          className="flex flex-col"
        >
          {numbers.split("").map((num) => (
            <span key={num} className="h-[1em]">
              {num}
            </span>
          ))}
        </motion.div>
      );
    };

    // 温度の数値をアニメーションで表示するコンポーネント
    const AnimatedNumber = ({ value }) => {
      const valueString = value.toFixed(2);
      return (
        <div className="flex h-[1em] overflow-hidden">
          {valueString.split("").map((char, index) => {
            if (char === ".") {
              return <span key={index} className="h-[1em]">.</span>;
            }
            return <DigitSlot key={index} digit={char} />;
          })}
        </div>
      );
    };

    // メインコンポーネント
    const Home = () => {
      const [error, setError] = useState("");
      const [data, setData] = useState({
        current_temp: 0,
        history: [],
      });
      const intervalRef = useRef(null);

      const fetchData = async () => {
        setError("");
        try {
          const response = await fetch(`/temp`); // ESP32上のAPIエンドポイントを直接叩く
          if (!response.ok) {
            throw new Error(`HTTPエラー: ${response.status}`);
          }
          const jsonData = await response.json();
          // ESP32から送られてくる履歴は古い順なので、グラフ用に逆順にする
          jsonData.history.reverse(); 
          setData(jsonData);
        } catch (e) {
          console.error("データ取得エラー:", e);
          setError("データの取得に失敗しました。ESP32との接続を確認してください。");
          if (intervalRef.current) {
            clearInterval(intervalRef.current);
          }
        }
      };
      
      // ページ読み込み時にデータ取得を開始
      useEffect(() => {
        fetchData(); // まず初回データを取得
        intervalRef.current = setInterval(fetchData, 5000); // 5秒ごとに更新

        return () => {
          if (intervalRef.current) {
            clearInterval(intervalRef.current);
          }
        };
      }, []);
      
      const cardVariants = {
        hidden: { opacity: 0, y: 50, scale: 0.95 },
        visible: {
          opacity: 1,
          y: 0,
          scale: 1,
          transition: { type: "spring", stiffness: 50, damping: 20, staggerChildren: 0.1 },
        },
      };

      const itemVariants = {
        hidden: { opacity: 0, y: 20 },
        visible: { opacity: 1, y: 0 },
      };

      return (
        <div className="bg-gray-900 text-white min-h-screen flex flex-col items-center justify-center p-4 sm:p-6 lg:p-8 font-sans relative overflow-hidden">
          <div className="absolute inset-0 z-0">
            <div className="absolute inset-0 bg-gradient-to-br from-gray-900 via-black to-blue-900/40 opacity-80"></div>
            <div className="absolute inset-0 bg-[url('data:image/svg+xml,%3Csvg%20xmlns%3D%22http%3A%2F%2Fwww.w3.org%2F2000%2Fsvg%22%20viewBox%3D%220%200%2032%2032%22%20width%3D%2232%22%20height%3D%2232%22%20fill%3D%22none%22%20stroke%3D%22%231f2937%22%3E%3Cpath%20d%3D%22M0%20.5H32V32%22%2F%3E%3C%2Fsvg%3E')] opacity-20"></div>
          </div>

          <main className="z-10 w-full max-w-5xl mx-auto">
             <motion.div key="dashboard" variants={cardVariants} initial="hidden" animate="visible" className="w-full">
                  <header className="flex justify-between items-center mb-6">
                    <motion.div variants={itemVariants} className="flex items-center text-2xl">
                       <Zap className="w-8 h-8 text-cyan-300 mr-3" />
                       <h1 className="font-bold">Refrigerator Temp Monitor</h1>
                    </motion.div>
                     {error && (
                      <motion.p initial={{ opacity: 0 }} animate={{ opacity: 1 }} className="text-red-400 text-sm text-center bg-red-900/50 px-4 py-2 rounded-lg">
                          {error}
                      </motion.p>
                    )}
                  </header>

                  <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
                      <motion.div
                        variants={itemVariants}
                        className="lg:col-span-1 bg-gray-800/50 backdrop-blur-md border border-gray-700 rounded-2xl p-6 flex flex-col  shadow-2xl shadow-blue-500/10"
                      >
                        <div className="flex items-center text-gray-300 mb-4">
                          <Thermometer className="w-7 h-7 mr-3 text-red-400" />
                          <h2 className="text-2xl font-semibold">現在の温度</h2>
                        </div>
                        <div className="flex-1 flex items-center justify-center">
                          <div className="text-7xl font-bold text-white tracking-tighter relative">
                            <AnimatedNumber value={data.current_temp} />
                            <span className="text-5xl text-gray-400 absolute -right-12 top-2">
                              °C
                            </span>
                          </div>
                        </div>
                      </motion.div>

                    <motion.div variants={itemVariants} className="lg:col-span-2 bg-gray-800/50 backdrop-blur-md border border-gray-700 rounded-2xl p-6 shadow-2xl shadow-blue-500/10">
                      <div className="flex items-center text-gray-300 mb-4">
                        <History className="w-6 h-6 mr-3 text-cyan-300" />
                        <h2 className="text-2xl font-semibold">温度履歴グラフ (過去5時間)</h2>
                      </div>
                      <div className="w-full h-64 sm:h-80">
                        <ResponsiveContainer width="100%" height="100%">
                          <AreaChart
                            data={data.history.map((temp, index) => ({ temp: temp, secondsAgo: index * 5 })).reverse()}
                            margin={{ top: 20, right: 30, left: 20, bottom: 5 }}
                          >
                            <defs>
                              <linearGradient id="colorTemp" x1="0" y1="0" x2="0" y2="1">
                                <stop offset="5%" stopColor="#22d3ee" stopOpacity={0.8} />
                                <stop offset="95%" stopColor="#22d3ee" stopOpacity={0} />
                              </linearGradient>
                            </defs>
                            <CartesianGrid strokeDasharray="3 3" stroke="#374151" />
                            <XAxis
                              dataKey="secondsAgo"
                              tick={{ fill: "#9ca3af" }}
                              stroke="#4b5563"
                              tickLine={false}
                              axisLine={false}
                              tickFormatter={(totalSeconds) => {
                                if (totalSeconds === 0) return '現在';
                                const minutes = Math.floor(totalSeconds / 60);
                                return `${minutes}分前`;
                              }}
                              interval="preserveStartEnd"
                              minTickGap={60}
                            />
                            <YAxis
                              tick={{ fill: "#9ca3af" }}
                              stroke="#4b5563"
                              domain={['dataMin - 1', 'dataMax + 1']}
                              tickLine={false}
                              axisLine={false}
                              tickFormatter={(temp) => `${temp.toFixed(1)}`}
                            />
                            <Tooltip content={<CustomTooltip />} cursor={{ stroke: "#06b6d4", strokeWidth: 1, strokeDasharray: "3 3" }} />
                            <Area
                              type="monotone"
                              dataKey="temp"
                              stroke="#22d3ee"
                              strokeWidth={2}
                              fillOpacity={1}
                              fill="url(#colorTemp)"
                              isAnimationActive={true}
                              animationDuration={800}
                              dot={false}
                              activeDot={{ r: 6, fill: "#06b6d4", stroke: "white", strokeWidth: 2 }}
                            />
                          </AreaChart>
                        </ResponsiveContainer>
                      </div>
                    </motion.div>
                  </div>
                </motion.div>
          </main>
        </div>
      );
    }

    // ReactアプリをDOMにマウント
    const container = document.getElementById("app");
    const root = createRoot(container);
    root.render(<Home />);
  </script>
</head>
<body>
  <div id="app"></div>
</body>
</html>
)rawliteral";


// 使用するピンを定義
#define I2C_SDA 1
#define I2C_SCL 2

// 温度取得と記録の設定
const unsigned long TEMP_READ_INTERVAL = 5000; // 温度取得の間隔 (5秒)
// 履歴の最大保持数 (5秒 * 3600回 = 5時間)
const int MAX_HISTORY_SIZE = 3600; 

// グローバル変数
Adafruit_AHTX0 aht;
WebServer server(80);
std::deque<float> tempHistory;
float currentTemperature = -999.0; // 初期値は異常値に
unsigned long previousMillis = 0;


void handleRoot() {
  // PROGMEMに保存したHTMLコンテンツを送信
  server.send(200, "text/html", index_html);
}



void handleTempRequest() {
  // JSONドキュメントを作成 (サイズは動的に確保)
  DynamicJsonDocument doc(JSON_ARRAY_SIZE(MAX_HISTORY_SIZE) + JSON_OBJECT_SIZE(2) + 200);

  // JSONオブジェクトに現在の気温を追加
  doc["current_temp"] = currentTemperature;

  // JSONオブジェクトに過去の気温履歴を配列として追加
  JsonArray history = doc.createNestedArray("history");
  for (float temp : tempHistory) {
    history.add(temp);
  }

  // JSONを文字列に変換して送信
  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}


void readAndStoreTemperature() {
  sensors_event_t humidity, temp;
  // センサーから値を取得
  if (aht.getEvent(&humidity, &temp)) {
    currentTemperature = temp.temperature;
    Serial.printf("Temperature: %.2f *C\n", currentTemperature);

    // 履歴に追加
    tempHistory.push_back(currentTemperature);

    // 履歴が最大サイズを超えたら、最も古いデータを削除
    if (tempHistory.size() > MAX_HISTORY_SIZE) {
      tempHistory.pop_front();
    }
  } else {
    Serial.println("Failed to read from AHT sensor.");
    currentTemperature = -999.0; // 読み取り失敗時は異常値
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  // I2C通信をカスタムピンで開始
  Wire.begin(I2C_SDA, I2C_SCL);

  // AHTセンサーの初期化
  if (!aht.begin()) {
    Serial.println("Failed to find AHT sensor. Check wiring.");
    while (1) {
      delay(10);
    }
  }
  Serial.println("AHT sensor found!");

  // Wi-Fiに接続
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Webサーバーのルーティング設定
  server.on("/", HTTP_GET, handleRoot); // ルートパスへのリクエストを処理
  server.on("/temp", HTTP_GET, handleTempRequest);

  // Webサーバーを開始
  server.begin();
  Serial.println("HTTP server started");

  // 起動時に一度温度を取得
  readAndStoreTemperature();
  previousMillis = millis();
}

void loop() {
  // Webサーバーのクライアントリクエストを処理
  server.handleClient();

  // 5秒経過したかチェック
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= TEMP_READ_INTERVAL) {
    // 経過時間を記録
    previousMillis = currentMillis;
    // 温度を取得して保存
    readAndStoreTemperature();
  }
}