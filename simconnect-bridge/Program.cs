using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.WebSockets;
using System.Runtime.InteropServices;
using System.Reflection;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using Microsoft.FlightSimulator.SimConnect;

internal static class Program
{
  private const int DefaultPort = 13375;
  private const int DefaultRateHz = 10;

  public static async Task Main(string[] args)
  {
    AppDomain.CurrentDomain.AssemblyResolve += (_, eventArgs) =>
    {
      if (!eventArgs.Name.StartsWith("Microsoft.FlightSimulator.SimConnect", StringComparison.OrdinalIgnoreCase))
      {
        return null;
      }
      string baseDir = AppContext.BaseDirectory;
      string[] candidates =
      {
        Path.Combine(baseDir, "Microsoft.FlightSimulator.SimConnect.dll"),
        Path.Combine(baseDir, "SimConnect.dll"),
        Path.Combine(baseDir, "lib", "Microsoft.FlightSimulator.SimConnect.dll"),
        Path.Combine(baseDir, "lib", "SimConnect.dll")
      };
      foreach (string path in candidates)
      {
        if (File.Exists(path))
        {
          return Assembly.LoadFrom(path);
        }
      }
      return null;
    };

    var settings = BridgeSettings.Parse(args, DefaultPort, DefaultRateHz);
    Console.WriteLine($"[Bridge] SimConnect WebSocket: ws://{settings.Host}:{settings.Port}");
    Console.WriteLine($"[Bridge] Rate: {settings.RateHz} Hz");
    Console.WriteLine($"[Bridge] Config: {settings.ConfigPath}");

    using var hub = new WebSocketHub(settings.Host, settings.Port);
    await hub.StartAsync();

    using var cts = new CancellationTokenSource();
    Console.CancelKeyPress += (_, e) =>
    {
      e.Cancel = true;
      cts.Cancel();
    };

    // Create worker with a Func that reloads config on each call
    var simWorker = new SimConnectWorker(hub, settings.RateHz, () => BridgeConfig.Load(settings.ConfigPath).SimVars);

    // Wire up reload command
    hub.OnReloadRequested += simWorker.RequestReload;
    while (!cts.Token.IsCancellationRequested)
    {
      try
      {
        await simWorker.RunAsync(cts.Token);
      }
      catch (FileNotFoundException)
      {
        Console.WriteLine("[Bridge] Managed SimConnect DLL missing. Copy it into simconnect-bridge/lib/.");
        try
        {
          await Task.Delay(2000, cts.Token);
        }
        catch (OperationCanceledException)
        {
          break;
        }
      }
    }

    await hub.StopAsync();
  }
}

internal sealed class BridgeSettings
{
  public string Host { get; set; } = "127.0.0.1";
  public int Port { get; set; } = 13375;
  public int RateHz { get; set; } = 2;
  public string? ConfigPath { get; set; }

  public static BridgeSettings Parse(string[] args, int defaultPort, int defaultRate)
  {
    var settings = new BridgeSettings
    {
      Port = defaultPort,
      RateHz = defaultRate
    };

    for (int i = 0; i < args.Length; i++)
    {
      string arg = args[i];
      if (arg == "--host" && i + 1 < args.Length)
      {
        settings.Host = args[++i];
      }
      else if (arg == "--port" && i + 1 < args.Length && int.TryParse(args[++i], out int port))
      {
        settings.Port = port;
      }
      else if (arg == "--rate" && i + 1 < args.Length && int.TryParse(args[++i], out int rate))
      {
        settings.RateHz = Math.Max(1, rate);
      }
      else if (arg == "--config" && i + 1 < args.Length)
      {
        settings.ConfigPath = args[++i];
      }
    }

    return settings;
  }
}

internal sealed class BridgeConfig
{
  public int Version { get; set; } = 1;

  [JsonPropertyName("simvars")]
  public List<SimVarDefinition> SimVars { get; set; } = new();

  private readonly struct SimVarDefaults
  {
    public SimVarDefaults(string unit, string type)
    {
      Unit = unit;
      Type = type;
    }

    public string Unit { get; }
    public string Type { get; }
  }

  private static readonly Dictionary<string, SimVarDefaults> SimVarDefaultsMap =
    new(StringComparer.OrdinalIgnoreCase)
    {
      { "AIRSPEED INDICATED", new SimVarDefaults("knots", "float64") },
      { "AIRSPEED TRUE", new SimVarDefaults("knots", "float64") },
      { "AIRSPEED MACH", new SimVarDefaults("mach", "float64") },
      { "GPS GROUND SPEED", new SimVarDefaults("knots", "float64") },
      { "PLANE ALTITUDE", new SimVarDefaults("feet", "float64") },
      { "INDICATED ALTITUDE", new SimVarDefaults("feet", "float64") },
      { "VERTICAL SPEED", new SimVarDefaults("feet per minute", "float64") },
      { "PLANE HEADING DEGREES MAGNETIC", new SimVarDefaults("degrees", "float64") },
      { "PLANE HEADING DEGREES TRUE", new SimVarDefaults("degrees", "float64") },
      { "FUEL TOTAL QUANTITY", new SimVarDefaults("gallons", "float64") },
      { "SIM ON GROUND", new SimVarDefaults("bool", "int32") }
    };

  public static BridgeConfig Load(string? path)
  {
    string? resolvedPath = ResolveConfigPath(path);
    if (resolvedPath != null)
    {
      try
      {
        string json = File.ReadAllText(resolvedPath);
        var config = JsonSerializer.Deserialize<BridgeConfig>(json);
        if (config != null)
        {
          config.SimVars = NormalizeSimVars(config.SimVars);
          if (config.SimVars.Count > 0)
          {
            return config;
          }
        }
      }
      catch (Exception ex)
      {
        Console.WriteLine($"[Bridge] Config load error: {ex.Message}");
      }
    }

    return new BridgeConfig
    {
      SimVars = DefaultSimVars()
    };
  }

  private static string? ResolveConfigPath(string? path)
  {
    if (!string.IsNullOrWhiteSpace(path) && File.Exists(path))
    {
      return path;
    }

    string candidate = Path.Combine(AppContext.BaseDirectory, "config.json");
    if (File.Exists(candidate))
    {
      return candidate;
    }

    return null;
  }

  private static List<SimVarDefinition> NormalizeSimVars(List<SimVarDefinition>? vars)
  {
    if (vars == null || vars.Count == 0) return new List<SimVarDefinition>();
    var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
    var result = new List<SimVarDefinition>();
    foreach (var entry in vars)
    {
      if (entry == null) continue;
      string entityId = (entry.EntityId ?? string.Empty).Trim();
      string simVar = (entry.SimVar ?? string.Empty).Trim();
      if (entityId.Length == 0 || simVar.Length == 0) continue;
      if (!seen.Add(entityId)) continue;
      string name = (entry.Name ?? string.Empty).Trim();
      string unit = (entry.Unit ?? string.Empty).Trim();
      string type = (entry.Type ?? string.Empty).Trim();
      if (SimVarDefaultsMap.TryGetValue(simVar, out var defaults))
      {
        if (unit.Length == 0) unit = defaults.Unit;
        if (type.Length == 0) type = defaults.Type;
      }
      if (type.Length == 0) type = "float64";
      result.Add(new SimVarDefinition
      {
        EntityId = entityId,
        Name = name.Length > 0 ? name : simVar,
        SimVar = simVar,
        Unit = unit,
        Type = type
      });
    }
    return result;
  }

  private static List<SimVarDefinition> DefaultSimVars()
  {
    return new List<SimVarDefinition>
    {
      new SimVarDefinition("sim.ias", "Indicated Airspeed", "AIRSPEED INDICATED", "knots", "float64"),
      new SimVarDefinition("sim.tas", "True Airspeed", "AIRSPEED TRUE", "knots", "float64"),
      new SimVarDefinition("sim.mach", "Mach", "AIRSPEED MACH", "mach", "float64"),
      new SimVarDefinition("sim.gs", "Ground Speed", "GPS GROUND SPEED", "knots", "float64"),
      new SimVarDefinition("sim.altitude", "Plane Altitude (MSL)", "PLANE ALTITUDE", "feet", "float64"),
      new SimVarDefinition("sim.altitude_indicated", "Indicated Altitude", "INDICATED ALTITUDE", "feet", "float64"),
      new SimVarDefinition("sim.vs", "Vertical Speed", "VERTICAL SPEED", "feet per minute", "float64"),
      new SimVarDefinition("sim.heading", "Heading (Mag)", "PLANE HEADING DEGREES MAGNETIC", "degrees", "float64"),
      new SimVarDefinition("sim.heading_true", "Heading (True)", "PLANE HEADING DEGREES TRUE", "degrees", "float64"),
      new SimVarDefinition("sim.fuel_total", "Fuel Total", "FUEL TOTAL QUANTITY", "gallons", "float64"),
      new SimVarDefinition("sim.on_ground", "On Ground", "SIM ON GROUND", "bool", "int32")
    };
  }
}

internal sealed class SimVarDefinition
{
  [JsonPropertyName("entity_id")]
  public string EntityId { get; set; } = string.Empty;

  [JsonPropertyName("name")]
  public string Name { get; set; } = string.Empty;

  [JsonPropertyName("simvar")]
  public string SimVar { get; set; } = string.Empty;

  [JsonPropertyName("unit")]
  public string Unit { get; set; } = string.Empty;

  [JsonPropertyName("type")]
  public string Type { get; set; } = "float64";

  [JsonIgnore]
  public SimVarDataType DataType => SimVarDataTypeParser.Parse(Type);

  public SimVarDefinition()
  {
  }

  public SimVarDefinition(string entityId, string name, string simVar, string unit, string type)
  {
    EntityId = entityId;
    Name = name;
    SimVar = simVar;
    Unit = unit;
    Type = type;
  }
}

internal enum SimVarDataType
{
  Float64,
  Int32,
  String32,
  String256
}

internal static class SimVarDataTypeParser
{
  public static SimVarDataType Parse(string? raw)
  {
    string value = (raw ?? string.Empty).Trim().ToLowerInvariant();
    return value switch
    {
      "int" => SimVarDataType.Int32,
      "int32" => SimVarDataType.Int32,
      "bool" => SimVarDataType.Int32,
      "string" => SimVarDataType.String32,
      "string32" => SimVarDataType.String32,
      "string256" => SimVarDataType.String256,
      "double" => SimVarDataType.Float64,
      "float" => SimVarDataType.Float64,
      "float64" => SimVarDataType.Float64,
      _ => SimVarDataType.Float64
    };
  }
}

internal sealed class WebSocketHub : IDisposable
{
  private readonly HttpListener _listener;
  private readonly List<WebSocket> _clients = new();
  private readonly object _lock = new();
  private readonly string _url;
  private CancellationTokenSource? _cts;

  public event Action? OnReloadRequested;

  public WebSocketHub(string host, int port)
  {
    _listener = new HttpListener();
    _url = $"http://{host}:{port}/";
    _listener.Prefixes.Add(_url);
  }

  public async Task StartAsync()
  {
    _cts = new CancellationTokenSource();
    _listener.Start();
    Console.WriteLine($"[Bridge] Listening on {_url}");
    _ = Task.Run(() => AcceptLoop(_cts.Token));
    await Task.CompletedTask;
  }

  public async Task StopAsync()
  {
    if (_cts != null)
    {
      _cts.Cancel();
      _cts.Dispose();
      _cts = null;
    }

    lock (_lock)
    {
      foreach (var ws in _clients)
      {
        try
        {
          ws.Abort();
        }
        catch
        {
          // Ignore cleanup errors.
        }
      }
      _clients.Clear();
    }

    if (_listener.IsListening)
    {
      _listener.Stop();
    }

    await Task.CompletedTask;
  }

  private async Task AcceptLoop(CancellationToken token)
  {
    while (!token.IsCancellationRequested)
    {
      HttpListenerContext? context = null;
      try
      {
        context = await _listener.GetContextAsync();
      }
      catch when (token.IsCancellationRequested)
      {
        break;
      }
      catch (Exception ex)
      {
        Console.WriteLine($"[Bridge] Listener error: {ex.Message}");
        await Task.Delay(500, token);
        continue;
      }

      if (context == null)
      {
        await Task.Delay(50, token);
        continue;
      }

      if (!context.Request.IsWebSocketRequest)
      {
        context.Response.StatusCode = 400;
        context.Response.Close();
        continue;
      }

      try
      {
        var wsContext = await context.AcceptWebSocketAsync(null);
        var socket = wsContext.WebSocket;
        lock (_lock)
        {
          _clients.Add(socket);
        }
        Console.WriteLine("[Bridge] WebSocket client connected");
        _ = Task.Run(() => ClientLoop(socket, token));
      }
      catch (Exception ex)
      {
        Console.WriteLine($"[Bridge] WebSocket accept error: {ex.Message}");
      }
    }
  }

  private async Task ClientLoop(WebSocket socket, CancellationToken token)
  {
    var buffer = new byte[1024];
    try
    {
      while (!token.IsCancellationRequested && socket.State == WebSocketState.Open)
      {
        var result = await socket.ReceiveAsync(new ArraySegment<byte>(buffer), token);
        if (result.MessageType == WebSocketMessageType.Close)
        {
          break;
        }
        if (result.MessageType == WebSocketMessageType.Text && result.Count > 0)
        {
          var message = Encoding.UTF8.GetString(buffer, 0, result.Count).Trim();
          HandleCommand(message);
        }
      }
    }
    catch
    {
      // Ignore client errors.
    }
    finally
    {
      lock (_lock)
      {
        _clients.Remove(socket);
      }
      try
      {
        socket.Abort();
      }
      catch
      {
        // Ignore cleanup errors.
      }
      Console.WriteLine("[Bridge] WebSocket client disconnected");
    }
  }

  private void HandleCommand(string message)
  {
    try
    {
      using var doc = JsonDocument.Parse(message);
      var root = doc.RootElement;
      if (root.TryGetProperty("command", out var cmdProp))
      {
        var cmd = cmdProp.GetString()?.ToLowerInvariant();
        if (cmd == "reload")
        {
          Console.WriteLine("[Bridge] Reload command received");
          OnReloadRequested?.Invoke();
        }
      }
    }
    catch
    {
      // Ignore invalid JSON
    }
  }

  public void Broadcast(string message)
  {
    var data = Encoding.UTF8.GetBytes(message);
    List<WebSocket> clients;
    lock (_lock)
    {
      clients = _clients.ToList();
    }

    foreach (var socket in clients)
    {
      if (socket.State != WebSocketState.Open)
      {
        lock (_lock)
        {
          _clients.Remove(socket);
        }
        continue;
      }

      _ = socket.SendAsync(new ArraySegment<byte>(data), WebSocketMessageType.Text, true, CancellationToken.None);
    }
  }

  public void Dispose()
  {
    _listener.Close();
  }
}

internal sealed class SimConnectWorker
{
  private const int DataDefinitionIdBase = 0;
  private const int DataRequestIdBase = 0;
  private readonly WebSocketHub _hub;
  private readonly int _sendIntervalMs;
  private readonly Func<List<SimVarDefinition>> _getSimVars;
  private List<SimVarDefinition> _simVars = new();
  private readonly Dictionary<int, object?> _lastValues = new();
  private readonly JsonSerializerOptions _jsonOptions = new JsonSerializerOptions
  {
    DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
  };

  private long _nextSendMs;
  private readonly Stopwatch _timer = Stopwatch.StartNew();
  private volatile bool _reloadRequested;

  public SimConnectWorker(WebSocketHub hub, int rateHz, Func<List<SimVarDefinition>> getSimVars)
  {
    _hub = hub;
    _sendIntervalMs = Math.Max(1, 1000 / Math.Max(1, rateHz));
    _getSimVars = getSimVars;
  }

  public void RequestReload()
  {
    _reloadRequested = true;
  }

  public async Task RunAsync(CancellationToken token)
  {
    while (!token.IsCancellationRequested)
    {
      // Reload SimVars before each connection attempt
      _simVars = _getSimVars();
      _reloadRequested = false;
      Console.WriteLine($"[Bridge] Loading {_simVars.Count} SimVars");

      try
      {
        using var simConnect = new SimConnect("Tab5 SimConnect Bridge", IntPtr.Zero, 0, null, 0);
        simConnect.OnRecvOpen += OnOpen;
        simConnect.OnRecvQuit += (_, _) => throw new SimConnectDisconnectedException();
        simConnect.OnRecvException += OnException;
        simConnect.OnRecvSimobjectData += OnData;

        SetupDefinitions(simConnect);

        Console.WriteLine("[Bridge] Connected to SimConnect");

        while (!token.IsCancellationRequested && !_reloadRequested)
        {
          simConnect.ReceiveMessage();
          Thread.Sleep(5);
        }

        if (_reloadRequested)
        {
          Console.WriteLine("[Bridge] Reloading SimVars...");
          continue;
        }
      }
      catch (SimConnectDisconnectedException)
      {
        Console.WriteLine("[Bridge] SimConnect disconnected");
      }
      catch (DllNotFoundException)
      {
        Console.WriteLine("[Bridge] SimConnect native DLL not found. Copy SimConnect.dll into simconnect-bridge/lib/.");
      }
      catch (Exception ex)
      {
        Console.WriteLine($"[Bridge] SimConnect error: {ex.Message}");
      }

      if (token.IsCancellationRequested)
      {
        break;
      }

      await Task.Delay(2000, token);
    }
  }

  private void SetupDefinitions(SimConnect simConnect)
  {
    _lastValues.Clear();
    if (_simVars.Count == 0)
    {
      Console.WriteLine("[Bridge] No SimVars configured.");
      return;
    }

    for (int i = 0; i < _simVars.Count; i++)
    {
      var entry = _simVars[i];
      var definitionId = (DataDefinitionId)(DataDefinitionIdBase + i);
      var requestId = (DataRequestId)(DataRequestIdBase + i);
      var dataType = MapDataType(entry.DataType);

      simConnect.AddToDataDefinition(definitionId, entry.SimVar, entry.Unit, dataType, 0.0f, SimConnect.SIMCONNECT_UNUSED);
      RegisterDefinition(simConnect, definitionId, entry.DataType);

      simConnect.RequestDataOnSimObject(
        requestId,
        definitionId,
        SimConnect.SIMCONNECT_OBJECT_ID_USER,
        SIMCONNECT_PERIOD.SIM_FRAME,
        SIMCONNECT_DATA_REQUEST_FLAG.DEFAULT,
        0,
        0,
        0);
    }
  }

  private void OnOpen(SimConnect sender, SIMCONNECT_RECV_OPEN data)
  {
    Console.WriteLine($"[Bridge] SimConnect: {data.szApplicationName}");
  }

  private void OnException(SimConnect sender, SIMCONNECT_RECV_EXCEPTION data)
  {
    Console.WriteLine($"[Bridge] SimConnect exception: {data.dwException}");
  }

  private void OnData(SimConnect sender, SIMCONNECT_RECV_SIMOBJECT_DATA data)
  {
    int requestId = (int)data.dwRequestID - DataRequestIdBase;
    if (requestId < 0 || requestId >= _simVars.Count)
    {
      return;
    }

    var entry = _simVars[requestId];
    object? value = ExtractValue(entry.DataType, data.dwData);
    _lastValues[requestId] = value;

    if (_timer.ElapsedMilliseconds < _nextSendMs)
    {
      return;
    }
    _nextSendMs = _timer.ElapsedMilliseconds + _sendIntervalMs;

    var sensors = new List<Sensor>();
    for (int i = 0; i < _simVars.Count; i++)
    {
      var def = _simVars[i];
      _lastValues.TryGetValue(i, out var lastValue);
      sensors.Add(new Sensor(def.EntityId, def.Name, def.Unit, lastValue));
    }

    var payload = new SimPayload
    {
      Timestamp = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(),
      Sensors = sensors
    };

    var json = JsonSerializer.Serialize(payload, _jsonOptions);
    _hub.Broadcast(json);
  }

  private static object? ExtractValue(SimVarDataType type, object? raw)
  {
    if (raw is object[] dataArray && dataArray.Length > 0)
    {
      raw = dataArray[0];
    }

    if (raw == null) return null;

    return type switch
    {
      SimVarDataType.Int32 => raw is SimValueInt iv ? iv.Value : raw,
      SimVarDataType.String32 => raw is SimValueString32 s32 ? s32.Value?.TrimEnd('\0') : raw,
      SimVarDataType.String256 => raw is SimValueString256 s256 ? s256.Value?.TrimEnd('\0') : raw,
      _ => raw is SimValueDouble dv ? dv.Value : raw
    };
  }

  private static SIMCONNECT_DATATYPE MapDataType(SimVarDataType type)
  {
    return type switch
    {
      SimVarDataType.Int32 => SIMCONNECT_DATATYPE.INT32,
      SimVarDataType.String32 => SIMCONNECT_DATATYPE.STRING32,
      SimVarDataType.String256 => SIMCONNECT_DATATYPE.STRING256,
      _ => SIMCONNECT_DATATYPE.FLOAT64
    };
  }

  private static void RegisterDefinition(SimConnect simConnect, DataDefinitionId definitionId, SimVarDataType type)
  {
    switch (type)
    {
      case SimVarDataType.Int32:
        simConnect.RegisterDataDefineStruct<SimValueInt>(definitionId);
        break;
      case SimVarDataType.String32:
        simConnect.RegisterDataDefineStruct<SimValueString32>(definitionId);
        break;
      case SimVarDataType.String256:
        simConnect.RegisterDataDefineStruct<SimValueString256>(definitionId);
        break;
      default:
        simConnect.RegisterDataDefineStruct<SimValueDouble>(definitionId);
        break;
    }
  }
}

internal sealed class SimConnectDisconnectedException : Exception
{
}

internal enum DataDefinitionId
{
  SimVarBase = 0
}

internal enum DataRequestId
{
  SimVarBase = 0
}

[StructLayout(LayoutKind.Sequential, Pack = 1)]
internal struct SimValueDouble
{
  public double Value;
}

[StructLayout(LayoutKind.Sequential, Pack = 1)]
internal struct SimValueInt
{
  public int Value;
}

[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack = 1)]
internal struct SimValueString32
{
  [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)]
  public string Value;
}

[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack = 1)]
internal struct SimValueString256
{
  [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
  public string Value;
}

internal sealed class Sensor
{
  [JsonPropertyName("entity_id")]
  public string EntityId { get; set; }

  [JsonPropertyName("name")]
  public string Name { get; set; }

  [JsonPropertyName("unit")]
  public string Unit { get; set; }

  [JsonPropertyName("value")]
  public object Value { get; set; }

  public Sensor(string entityId, string name, string unit, object value)
  {
    EntityId = entityId;
    Name = name;
    Unit = unit;
    Value = value;
  }
}

internal sealed class SimPayload
{
  [JsonPropertyName("type")]
  public string Type { get; set; } = "sim_metrics";

  [JsonPropertyName("timestamp")]
  public long Timestamp { get; set; }

  [JsonPropertyName("sensors")]
  public List<Sensor> Sensors { get; set; } = new();
}
