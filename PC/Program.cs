using System;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Runtime.InteropServices;
using System.Threading;
using System.Reflection;
using WindowsInput.Native;
using WindowsInput;
using System.Diagnostics;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using Microsoft.Toolkit.Uwp.Notifications;

namespace IR_Controller
{

    internal class Program
    {
        class ProcessInfo
        {
            public string Name { get; set; }
            public List<KeyInfo> Keys { get; set; }
        }

        class KeyInfo
        {
            public string Command { get; set; }
            public string Key { get; set; }
            public string Function { get; set; }
        }

        static List<ProcessInfo> processInfos;

        static InputSimulator inputSimulator = new InputSimulator();

        static float temperature = float.NaN;


        static void Main(string[] args)
        {
            UdpClient udpClient = new UdpClient(1234); // UDP port to listen for commands
            IPEndPoint remoteEP = new IPEndPoint(IPAddress.Any, 0);

            Console.WriteLine("PC Control App: Waiting for commands...");

            processInfos = LoadProcessInfos("commands.json");


            while (true)
            {
                byte[] commandBytes = udpClient.Receive(ref remoteEP);
                string command = Encoding.ASCII.GetString(commandBytes);
                command = command.Trim();

                //Console.WriteLine("Command received: " + command);

                switch (ParseJson(command).type)
                {
                    case "ir":
                        Process activeProcess = GetActiveProcess();

                        string ircode = ParseJson(command).value.ToString();

                        if (activeProcess != null)
                        {
                            string activeAppName = activeProcess.ProcessName.ToLower();

                            ProcessInfo processInfo = processInfos.Find(p => p.Name.ToLower() == activeAppName);
                            if (processInfo != null)
                            {
                                KeyInfo keyInfo = processInfo.Keys.FirstOrDefault(k => k.Command == ircode);
                                if (keyInfo != null)
                                {
                                    SimulateKey(keyInfo, ircode, activeAppName);
                                }
                                else
                                {
                                    ProcessDefaultCommand(ircode);
                                }

                                /*foreach (KeyInfo keyInfo in processInfo.Keys)
                            {
                                if (keyInfo.Command == command)
                                {

                                    break;
                                }
                            }*/
                            }
                            else
                            {
                                // Default commands for other applications
                                ProcessDefaultCommand(ircode);
                            }
                        }
                        else
                        {
                            // Default commands when no application is active
                            ProcessDefaultCommand(ircode);
                        }

                        break;
                    case "temperature":
                        temperature = Convert.ToSingle(ParseJson(command).value);
                        break;
                    default:
                        break;
                }


            }
        }

        static void ProcessDefaultCommand(string command)
        {
            ProcessInfo processInfo = processInfos.Find(p => p.Name.ToLower() == "default");
            if (processInfo != null)
            {
                KeyInfo keyInfo = processInfo.Keys.FirstOrDefault(k => k.Command == command);
                if (keyInfo != null)
                {
                    SimulateKey(keyInfo, command);
                } else
                {
                    Console.WriteLine("Empty command received: " + command);
                }
            }
        }

        static Process GetActiveProcess()
        {
            IntPtr hwnd = User32.GetForegroundWindow();
            uint processId;
            User32.GetWindowThreadProcessId(hwnd, out processId);
            return Process.GetProcessById((int)processId);
        }

        static void SimulateKey(KeyInfo keyInfo, string command = "N/A", string appname = "Default")
        {
            Console.WriteLine($"{appname}: Command '{command}' - Action '{keyInfo.Key}'");

            /*if (Enum.TryParse(key, out VirtualKeyCode keyCode))
            {
                inputSimulator.Keyboard.KeyPress(keyCode);
            }
            else
            {
                Console.WriteLine($"Invalid key '{key}'");
            }*/


            if (!string.IsNullOrEmpty(keyInfo.Function))
            {
                Type programType = typeof(Program);
                MethodInfo method = programType.GetMethod(keyInfo.Function);

                if (method != null)
                {
                    Console.WriteLine($"Executing function '{keyInfo.Function}'");
                    method.Invoke(null, new object[] { });
                    return;
                }
                else
                {
                    Console.WriteLine($"Function '{keyInfo.Function}' not found");
                }
            }

            if (Enum.TryParse(keyInfo.Key, out VirtualKeyCode keyCode))
            {
                //VirtualKeyCode.VOLUME_MUTE
                inputSimulator.Keyboard.KeyPress(keyCode);
            }
            else
            {
                Console.WriteLine($"Invalid key '{keyInfo.Key}'");
            }
        }

        static List<ProcessInfo> LoadProcessInfos(string filePath)
        {
            try
            {
                string json = File.ReadAllText(filePath);
                return JsonConvert.DeserializeObject<List<ProcessInfo>>(json);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error loading process infos from file '{filePath}': {ex.Message}");
                return new List<ProcessInfo>();
            }
        }

        public static void DebugInfo()
        {
            Process activeProcess = GetActiveProcess();
            string activeAppName = activeProcess.ProcessName.ToLower();
            Console.WriteLine("===== DebugInfo =====");
            Console.WriteLine($"Current Apps: {activeAppName}");
            Console.WriteLine("=====================");
        }

        public static void PrintTemperature()
        {
            if (!float.IsNaN(temperature))
            {
                Console.WriteLine($"Temperature: {temperature.ToString("F2")}°C");
                ShowNotification("ESP8266", $"Temperature: {temperature.ToString("F2")}°C");
            } else
            {
                Console.WriteLine("Values were not obtained!");
            }
        }

        public static (string type, object value) ParseJson(string jsonString)
        {
            JObject jsonObject = JObject.Parse(jsonString);

            foreach (var property in jsonObject.Properties())
            {
                string type = property.Name;
                JToken value = property.Value;
                return (type, value);
            }
            return (null, null);
        }
     
        public static void ShowNotification(string title, string message)
        {
            var notify1 = new ToastContentBuilder().AddText("ESP8266").AddText($"Temperature: {temperature.ToString("F2")}°C");
            //.AddHeroImage(new Uri("https://cdn-icons-png.flaticon.com/64/10398/10398851.png"));
            notify1.Show();
        }

    }

    static class User32
    {
        [System.Runtime.InteropServices.DllImport("user32.dll")]
        public static extern IntPtr GetForegroundWindow();

        [System.Runtime.InteropServices.DllImport("user32.dll")]
        public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);
    }
}