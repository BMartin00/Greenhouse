package banyanszki.martin.greenhouse;

import android.graphics.Color;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.View;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.ImageButton;
import android.widget.Toast;

import androidx.activity.EdgeToEdge;
import androidx.appcompat.app.AppCompatActivity;

import org.eclipse.paho.client.mqttv3.IMqttActionListener;
import org.eclipse.paho.client.mqttv3.IMqttAsyncClient;
import org.eclipse.paho.client.mqttv3.IMqttDeliveryToken;
import org.eclipse.paho.client.mqttv3.IMqttToken;
import org.eclipse.paho.client.mqttv3.MqttAsyncClient;
import org.eclipse.paho.client.mqttv3.MqttCallback;
import org.eclipse.paho.client.mqttv3.MqttClient;
import org.eclipse.paho.client.mqttv3.MqttConnectOptions;
import org.eclipse.paho.client.mqttv3.MqttException;
import org.eclipse.paho.client.mqttv3.MqttMessage;
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence;
import org.json.JSONException;
import org.json.JSONObject;

import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

public class MainActivity extends AppCompatActivity {
    private static final String TAG = "GreenhouseControl";
    private static final String BROKER = "MQTT_IP";
    private static final String TOPIC = "greenhouse/sensors/data";
    private static final String CONTROL_TOPIC = "greenhouse/control";
    private static final String STATUS_TOPIC = "greenhouse/status";

    private IMqttAsyncClient mqttClient;
    private MemoryPersistence persistence;
    private TextView sensorValuesTv;
    private ImageButton fanBtn;
    private ImageButton waterBtn;
    private ImageButton uvBtn;
    private Switch fanModeSwitch;
    private Switch waterModeSwitch;
    private Switch uvModeSwitch;
    private String clientId;

    private boolean fanOn = false;
    private boolean waterOn = false;
    private boolean uvOn = false;
    private boolean fanAutoMode = true;
    private boolean waterAutoMode = true;
    private boolean uvAutoMode = true;
    private boolean isProcessingControl = false;
    private Handler handler = new Handler();

    // Sensor values
    private float currentTemperature = 0;
    private float currentHumidity = 0;
    private int currentSoilMoisture = 0;
    private int currentLightLevel = 0;

    private SimpleDateFormat timeFormat = new SimpleDateFormat("HH:mm:ss", Locale.getDefault());

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        EdgeToEdge.enable(this);
        setContentView(R.layout.activity_main);

        initializeViews();
        setupClickListeners();

        clientId = MqttClient.generateClientId();
        persistence = new MemoryPersistence();

        updateUI();
    }

    private void initializeViews() {
        sensorValuesTv = findViewById(R.id.sensor_values_tv);
        fanBtn = findViewById(R.id.fan_btn);
        waterBtn = findViewById(R.id.water_btn);
        uvBtn = findViewById(R.id.uv_btn);

        fanModeSwitch = findViewById(R.id.fanModeSwitch);
        waterModeSwitch = findViewById(R.id.waterModeSwitch);
        uvModeSwitch = findViewById(R.id.uvModeSwitch);

        // Set initial mode switch states
        fanModeSwitch.setChecked(fanAutoMode);
        waterModeSwitch.setChecked(waterAutoMode);
        uvModeSwitch.setChecked(uvAutoMode);

        fanModeSwitch.setText(fanAutoMode ? "Auto" : "Manual");
        waterModeSwitch.setText(waterAutoMode ? "Auto" : "Manual");
        uvModeSwitch.setText(uvAutoMode ? "Auto" : "Manual");
    }

    private void setupClickListeners() {
        fanModeSwitch.setOnCheckedChangeListener((buttonView, isChecked) -> {
            fanAutoMode = isChecked;
            sendMode("fan_mode", fanAutoMode);
            fanModeSwitch.setText(fanAutoMode ? "Auto" : "Manual");
            updateButtonStates();
        });

        waterModeSwitch.setOnCheckedChangeListener((buttonView, isChecked) -> {
            waterAutoMode = isChecked;
            sendMode("water_mode", waterAutoMode);
            waterModeSwitch.setText(waterAutoMode ? "Auto" : "Manual");
            updateButtonStates();
        });

        uvModeSwitch.setOnCheckedChangeListener((buttonView, isChecked) -> {
            uvAutoMode = isChecked;
            sendMode("uv_mode", uvAutoMode);
            uvModeSwitch.setText(uvAutoMode ? "Auto" : "Manual");
            updateButtonStates();
        });
    }

    private MqttCallback mqttCallback = new MqttCallback() {
        @Override
        public void messageArrived(String topic, MqttMessage message) throws Exception {
            String payload = new String(message.getPayload(), StandardCharsets.UTF_8);
            Log.d(TAG, "MQTT Message received - Topic: " + topic + ", Payload: " + payload);

            runOnUiThread(() -> {
                try {
                    if (topic.equals(TOPIC)) {
                        parseSensorData(payload);
                        updateSensorDisplay();

                    } else if (topic.equals(STATUS_TOPIC)) {
                        parseStatusData(payload);
                        updateUI();
                        isProcessingControl = false;
                    }

                } catch (Exception e) {
                    Log.e(TAG, "Error processing message", e);
                }
            });
        }

        @Override
        public void deliveryComplete(IMqttDeliveryToken token) {
            Log.d(TAG, "Message delivery complete");
        }

        @Override
        public void connectionLost(Throwable cause) {
            Log.e(TAG, "MQTT connection lost", cause);
            runOnUiThread(() -> {
                Toast.makeText(MainActivity.this, "Connection lost", Toast.LENGTH_SHORT).show();
                isProcessingControl = false;
                updateButtonStates();
            });
        }
    };

    private void parseSensorData(String jsonPayload) {
        try {
            JSONObject json = new JSONObject(jsonPayload);

            String tempStr = json.optString("temperature", "--");
            String humidStr = json.optString("humidity", "--");
            currentSoilMoisture = json.optInt("soil_moisture", 0);
            currentLightLevel = json.optInt("light_raw", 0);

            // Parse temperature and humidity
            try {
                currentTemperature = Float.parseFloat(tempStr);
            } catch (NumberFormatException e) {
                currentTemperature = 0;
            }

            try {
                currentHumidity = Float.parseFloat(humidStr);
            } catch (NumberFormatException e) {
                currentHumidity = 0;
            }

            // Check for modes in sensor data
            if (json.has("fan_mode")) {
                String mode = json.getString("fan_mode");
                fanAutoMode = mode.equals("auto");
            }

            if (json.has("water_mode")) {
                String mode = json.getString("water_mode");
                waterAutoMode = mode.equals("auto");
            }

            if (json.has("uv_mode")) {
                String mode = json.getString("uv_mode");
                uvAutoMode = mode.equals("auto");
            }

            // Update switches on UI thread
            runOnUiThread(() -> {
                fanModeSwitch.setChecked(fanAutoMode);
                waterModeSwitch.setChecked(waterAutoMode);
                uvModeSwitch.setChecked(uvAutoMode);

                fanModeSwitch.setText(fanAutoMode ? "Auto" : "Manual");
                waterModeSwitch.setText(waterAutoMode ? "Auto" : "Manual");
                uvModeSwitch.setText(uvAutoMode ? "Auto" : "Manual");
            });

        } catch (JSONException e) {
            Log.e(TAG, "Sensor JSON parsing error", e);
        }
    }

    private void parseStatusData(String jsonPayload) {
        try {
            JSONObject json = new JSONObject(jsonPayload);

            fanOn = json.getString("fan").equals("on");
            waterOn = json.getString("water").equals("on");
            uvOn = json.getString("uv").equals("on");

            if (json.has("fan_mode")) {
                String mode = json.getString("fan_mode");
                fanAutoMode = mode.equals("auto");
            }

            if (json.has("water_mode")) {
                String mode = json.getString("water_mode");
                waterAutoMode = mode.equals("auto");
            }

            if (json.has("uv_mode")) {
                String mode = json.getString("uv_mode");
                uvAutoMode = mode.equals("auto");
            }

            if (json.has("temperature")) {
                float temp = (float) json.getDouble("temperature");
                if (temp > -900) {  // Valid temperature
                    currentTemperature = temp;
                }
            }

            if (json.has("soil_moisture")) {
                currentSoilMoisture = json.getInt("soil_moisture");
            }

            if (json.has("light_raw")) {
                currentLightLevel = json.getInt("light_raw");
            }

            Log.d(TAG, String.format("Status - Fan: %s(%s), Water: %s(%s), UV: %s(%s)",
                    fanOn ? "ON" : "OFF", fanAutoMode ? "Auto" : "Manual",
                    waterOn ? "ON" : "OFF", waterAutoMode ? "Auto" : "Manual",
                    uvOn ? "ON" : "OFF", uvAutoMode ? "Auto" : "Manual"));

            // Update switches on UI thread
            runOnUiThread(() -> {
                fanModeSwitch.setChecked(fanAutoMode);
                waterModeSwitch.setChecked(waterAutoMode);
                uvModeSwitch.setChecked(uvAutoMode);

                fanModeSwitch.setText(fanAutoMode ? "Auto" : "Manual");
                waterModeSwitch.setText(waterAutoMode ? "Auto" : "Manual");
                uvModeSwitch.setText(uvAutoMode ? "Auto" : "Manual");
            });

        } catch (JSONException e) {
            Log.e(TAG, "Status JSON parsing error", e);
        }
    }

    private void updateSensorDisplay() {
        String tempDisplay = currentTemperature > -900 ?
                String.format("%.1f°C", currentTemperature) : "--°C";
        String humidDisplay = currentHumidity > -900 ?
                String.format("%.1f%%", currentHumidity) : "--%";

        // Create mode indicators
        String fanModeIcon = fanAutoMode ? "🤖" : "👤";
        String waterModeIcon = waterAutoMode ? "🤖" : "👤";
        String uvModeIcon = uvAutoMode ? "🤖" : "👤";

        // Create status indicators with colors
        String fanStatus = String.format("%s Fan: %s", fanModeIcon, fanOn ? "ON" : "OFF");
        String waterStatus = String.format("%s Water: %s", waterModeIcon, waterOn ? "ON" : "OFF");
        String uvStatus = String.format("%s UV: %s", uvModeIcon, uvOn ? "ON" : "OFF");

        String displayText = "🌡 Temperature: " + tempDisplay + "\n" +
                "💧 Humidity: " + humidDisplay + "\n" +
                "🌱 Soil Moisture: " + currentSoilMoisture + "%\n" +
                "☀️ Light Level: " + currentLightLevel + "\n\n" +
                fanStatus + "\n" +
                waterStatus + "\n" +
                uvStatus + "\n\n" +
                "Last update: " + timeFormat.format(new Date());

        sensorValuesTv.setText(displayText);
    }

    private void updateUI() {
        updateButtonStates();
        updateSensorDisplay();
    }

    private void updateButtonStates() {
        fanBtn.setSelected(fanOn);
        waterBtn.setSelected(waterOn);
        uvBtn.setSelected(uvOn);

        fanBtn.setBackgroundColor(fanOn ? Color.GREEN : Color.RED);
        waterBtn.setBackgroundColor(waterOn ? Color.GREEN : Color.RED);
        uvBtn.setBackgroundColor(uvOn ? Color.GREEN : Color.RED);

        // Enable/disable buttons based on mode
        fanBtn.setEnabled(!fanAutoMode);
        waterBtn.setEnabled(!waterAutoMode);
        uvBtn.setEnabled(!uvAutoMode);

        // Set button alpha based on mode
        fanBtn.setAlpha(fanAutoMode ? 0.5f : 1.0f);
        waterBtn.setAlpha(waterAutoMode ? 0.5f : 1.0f);
        uvBtn.setAlpha(uvAutoMode ? 0.5f : 1.0f);

        fanBtn.setContentDescription((fanAutoMode ? "Auto - " : "Manual - ") + (fanOn ? "Fan On" : "Fan Off"));
        waterBtn.setContentDescription((waterAutoMode ? "Auto - " : "Manual - ") + (waterOn ? "Water On" : "Water Off"));
        uvBtn.setContentDescription((uvAutoMode ? "Auto - " : "Manual - ") + (uvOn ? "UV Light On" : "UV Light Off"));
    }

    @Override
    protected void onResume() {
        super.onResume();
        connectToMQTT();
    }

    private void connectToMQTT() {
        try {
            mqttClient = new MqttAsyncClient(BROKER, clientId, persistence);

            MqttConnectOptions options = new MqttConnectOptions();
            options.setCleanSession(true);
            options.setAutomaticReconnect(true);
            options.setConnectionTimeout(10);
            options.setKeepAliveInterval(20);

            mqttClient.connect(options, null, new IMqttActionListener() {
                @Override
                public void onSuccess(IMqttToken asyncActionToken) {
                    Log.d(TAG, "MQTT connected successfully");
                    try {
                        mqttClient.subscribe(TOPIC, 0);
                        mqttClient.subscribe(STATUS_TOPIC, 0);
                        mqttClient.setCallback(mqttCallback);

                        // Request current status
                        handler.postDelayed(() -> requestCurrentStatus(), 1000);

                    } catch (MqttException e) {
                        Log.e(TAG, "Subscription error", e);
                    }
                }

                @Override
                public void onFailure(IMqttToken asyncActionToken, Throwable exception) {
                    Log.e(TAG, "MQTT connection failed", exception);
                    runOnUiThread(() -> {
                        Toast.makeText(MainActivity.this,
                                "Failed to connect: " + exception.getMessage(),
                                Toast.LENGTH_LONG).show();
                    });
                }
            });
        } catch (MqttException e) {
            Log.e(TAG, "MQTT client creation error", e);
            Toast.makeText(this, "Error creating MQTT client: " + e.getMessage(),
                    Toast.LENGTH_LONG).show();
        }
    }

    private void requestCurrentStatus() {
        try {
            JSONObject json = new JSONObject();
            json.put("request", "status");
            sendControlMessage(json, false);
            Log.d(TAG, "Requested current status");
        } catch (JSONException e) {
            Log.e(TAG, "Error creating status request", e);
        }
    }

    private void sendMode(String modeKey, boolean autoMode) {
        try {
            JSONObject json = new JSONObject();
            json.put(modeKey, autoMode ? "auto" : "manual");
            sendControlMessage(json, true);
            Log.d(TAG, modeKey + " set to: " + (autoMode ? "Auto" : "Manual"));
        } catch (JSONException e) {
            Log.e(TAG, "Error sending mode", e);
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        try {
            if (mqttClient != null && mqttClient.isConnected()) {
                mqttClient.disconnect();
            }
        } catch (MqttException e) {
            Log.e(TAG, "Error disconnecting MQTT", e);
        }
    }

    public void controlFan(View view) {
        if (isProcessingControl) {
            Log.d(TAG, "Control already in progress, ignoring fan click");
            return;
        }

        if (fanAutoMode) {
            // In auto mode, switch to manual mode first
            Toast.makeText(this, "Switching Fan to Manual mode...", Toast.LENGTH_SHORT).show();

            fanAutoMode = false;
            runOnUiThread(() -> {
                fanModeSwitch.setChecked(false);
                fanModeSwitch.setText("Manual");
                fanBtn.setEnabled(true);
                fanBtn.setAlpha(1.0f);
            });

            // Send mode change
            sendMode("fan_mode", false);

            // Wait for mode change then send fan command
            handler.postDelayed(() -> {
                sendComponentCommand("fan", fanOn);
                isProcessingControl = false;
            }, 1000);

        } else {
            // Already in manual mode, just send command
            sendComponentCommand("fan", fanOn);
            isProcessingControl = false;
        }
    }

    public void controlWater(View view) {
        if (isProcessingControl) return;

        if (waterAutoMode) {
            // In auto mode, switch to manual mode first
            Toast.makeText(this, "Switching Water to Manual mode...", Toast.LENGTH_SHORT).show();

            waterAutoMode = false;
            runOnUiThread(() -> {
                waterModeSwitch.setChecked(false);
                waterModeSwitch.setText("Manual");
                waterBtn.setEnabled(true);
                waterBtn.setAlpha(1.0f);
            });

            // Send mode change
            sendMode("water_mode", false);

            // Wait for mode change then send water command
            handler.postDelayed(() -> {
                sendComponentCommand("water", waterOn);
                isProcessingControl = false;
            }, 1000);

        } else {
            // Already in manual mode, just send command
            sendComponentCommand("water", waterOn);
            isProcessingControl = false;
        }
    }

    public void controlUv(View view) {
        if (isProcessingControl) return;

        if (uvAutoMode) {
            // In auto mode, switch to manual mode first
            Toast.makeText(this, "Switching UV to Manual mode...", Toast.LENGTH_SHORT).show();

            uvAutoMode = false;
            runOnUiThread(() -> {
                uvModeSwitch.setChecked(false);
                uvModeSwitch.setText("Manual");
                uvBtn.setEnabled(true);
                uvBtn.setAlpha(1.0f);
            });

            // Send mode change
            sendMode("uv_mode", false);

            // Wait for mode change then send UV command
            handler.postDelayed(() -> {
                sendComponentCommand("uv", uvOn);
                isProcessingControl = false;
            }, 1000);

        } else {
            // Already in manual mode, just send command
            sendComponentCommand("uv", uvOn);
            isProcessingControl = false;
        }
    }

    private void sendComponentCommand(String component, boolean currentState) {
        JSONObject json = new JSONObject();
        try {
            json.put(component, currentState ? "off" : "on");
            sendControlMessage(json, true);
            Log.d(TAG, component + " command sent: " + (currentState ? "off" : "on"));
        } catch (JSONException e) {
            Log.e(TAG, "Error creating " + component + " command JSON", e);
            isProcessingControl = false;
        }
    }

    private void sendControlMessage(JSONObject json, boolean showToast) {
        try {
            if (mqttClient == null || !mqttClient.isConnected()) {
                Log.e(TAG, "MQTT client not connected");
                runOnUiThread(() -> {
                    Toast.makeText(this, "Not connected to greenhouse", Toast.LENGTH_SHORT).show();
                    isProcessingControl = false;
                });
                return;
            }

            String payload = json.toString();
            MqttMessage message = new MqttMessage(payload.getBytes(StandardCharsets.UTF_8));
            message.setQos(1);
            message.setRetained(false);

            mqttClient.publish(CONTROL_TOPIC, message);

            Log.d(TAG, "Control message sent: " + payload);

            if (showToast) {
                runOnUiThread(() -> {
                    Toast.makeText(this, "Command sent", Toast.LENGTH_SHORT).show();
                });
            }

        } catch (MqttException e) {
            Log.e(TAG, "Error sending control message", e);
            runOnUiThread(() -> {
                Toast.makeText(this, "Failed to send command", Toast.LENGTH_SHORT).show();
                isProcessingControl = false;
            });
        }
    }
}