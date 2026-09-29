window.sensorSignalR = {
  connection: null,

  start: async function (dotnetHelper) {
    console.log("SIGNALR: start() dipanggil");

    try {
      if (typeof signalR === "undefined") {
        console.error("SIGNALR: library tidak ditemukan");
        return;
      }

      console.log("SIGNALR: membuat HubConnection...");

      const connection = new signalR.HubConnectionBuilder()
        .withUrl("/sensorhub")
        .withAutomaticReconnect()
        .configureLogging(signalR.LogLevel.Information)
        .build();

      this.connection = connection;

      // ==========================================
      // MQTT TELEMETRY
      // ==========================================

      connection.on("ReceiveMqttTelemetry", async function (data) {
        console.log("SIGNALR MQTT TELEMETRY RECEIVED:", data);

        try {
          await dotnetHelper.invokeMethodAsync("ReceiveMqttTelemetry", data);

          console.log("SIGNALR: MQTT telemetry berhasil dikirim ke Blazor");
        } catch (error) {
          console.error(
            "SIGNALR: gagal mengirim MQTT telemetry ke Blazor:",
            error,
          );
        }
      });

      // ==========================================
      // MQTT DEVICE STATUS
      // ==========================================

      connection.on("ReceiveDeviceStatus", async function (data) {
        console.log("SIGNALR MQTT STATUS RECEIVED:", data);

        try {
          await dotnetHelper.invokeMethodAsync("ReceiveDeviceStatus", data);

          console.log("SIGNALR: MQTT status berhasil dikirim ke Blazor");
        } catch (error) {
          console.error(
            "SIGNALR: gagal mengirim MQTT status ke Blazor:",
            error,
          );
        }
      });

      // ==========================================
      // LEGACY SENSOR DATA
      // ==========================================

      connection.on("ReceiveSensorData", async function (data) {
        console.log("SIGNALR DATA RECEIVED:", data);

        try {
          await dotnetHelper.invokeMethodAsync("ReceiveSensorData", data);

          console.log("SIGNALR: data berhasil dikirim ke Blazor");
        } catch (error) {
          console.error("SIGNALR: gagal memanggil Blazor:", error);
        }
      });

      // ==========================================
      // CONNECTION EVENTS
      // ==========================================

      connection.onclose(function (error) {
        console.error("SIGNALR CLOSED:", error);
      });

      connection.onreconnecting(function (error) {
        console.warn("SIGNALR RECONNECTING:", error);
      });

      connection.onreconnected(function (connectionId) {
        console.log("SIGNALR RECONNECTED:", connectionId);
      });

      // ==========================================
      // START CONNECTION
      // ==========================================

      console.log("SIGNALR: mencoba connection.start()...");

      await connection.start();

      console.log("SIGNALR STARTED:", connection.state);
    } catch (error) {
      console.error("SIGNALR START FAILED:", error);

      console.error("SIGNALR ERROR MESSAGE:", error?.message);

      console.error("SIGNALR ERROR STACK:", error?.stack);
    }
  },
};
