using MQTTnet;
using MQTTnet.Client.Options;
using MQTTnet.Extensions.ManagedClient;
using System;
using System.Net;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace iot_BME280_influx_exporter
{
    class Program
    {
        private static IManagedMqttClient client;
        static ManualResetEvent _quitEvent = new ManualResetEvent(false);
        static void Main(string[] args)
        {
            WebClient webclient = new WebClient();
            Console.WriteLine("Hello World!");
            Task.Run(async () =>
            {
                await ConnectAsync();
                client.UseConnectedHandler(e =>
                {
                    Console.WriteLine("Connected successfully with MQTT Brokers.");
                });
                client.UseDisconnectedHandler(e =>
                {
                    Console.WriteLine("Disconnected from MQTT Brokers.");
                });
                client.UseApplicationMessageReceivedHandler(e =>
                {
                    try
                    {
                        string topic = e.ApplicationMessage.Topic;
                        if (string.IsNullOrWhiteSpace(topic) == false)
                        {
                            string payload = Encoding.UTF8.GetString(e.ApplicationMessage.Payload);
                            Console.WriteLine($"Topic: {topic}. Message Received: {payload}");
                            string[] topicParts = topic.Split("/");
                            // "temperature, type="weather", device="mcu12390u0adaksnjl", value=25.23, timestamp (optional, nanosecond unix time)
                            string influxPayload = topicParts[2]+",type="+topicParts[1]+",device="+topicParts[3]+" value="+payload;
                            var response = webclient.UploadString("http://192.168.10.37:8086/write?db=iot", influxPayload);
                            //Console.WriteLine(response);
                        }
                    }
                    catch (Exception ex)
                    {
                        Console.WriteLine(ex.Message, ex);
                    }
                });

                // Subscribe to channels
                _ = SubscribeAsync("iot/weather/+/+", 1);
            });
            // Prevent immediate exit
            _quitEvent.WaitOne();
        }
        /// <summary>
        /// Connect to broker.
        /// </summary>
        /// <returns>Task.</returns>
        public static async Task ConnectAsync()
        {
            string clientId = Guid.NewGuid().ToString();
            string mqttURI = "192.168.10.31";
            string mqttUser = "danielv";
            string mqttPassword = "aq12wsxc";
            int mqttPort = 1883;
            bool mqttSecure = false;
            var messageBuilder = new MqttClientOptionsBuilder()
                .WithClientId(clientId)
                .WithCredentials(mqttUser, mqttPassword)
                .WithTcpServer(mqttURI, mqttPort)
                .WithCleanSession();
            var options = mqttSecure
              ? messageBuilder
                .WithTls()
                .Build()
              : messageBuilder
                .Build();
            var managedOptions = new ManagedMqttClientOptionsBuilder()
                .WithAutoReconnectDelay(TimeSpan.FromSeconds(5))
                .WithClientOptions(options)
                .Build();
            client = new MqttFactory().CreateManagedMqttClient();
            await client.StartAsync(managedOptions);
        }
        /// <summary>
        /// Publish Message.
        /// </summary>
        /// <param name="topic">Topic.</param>
        /// <param name="payload">Payload.</param>
        /// <param name="retainFlag">Retain flag.</param>
        /// <param name="qos">Quality of Service.</param>
        /// <returns>Task.</returns>
        public static async Task PublishAsync(string topic, string payload, bool retainFlag = true, int qos = 1) =>
          await client.PublishAsync(new MqttApplicationMessageBuilder()
            .WithTopic(topic)
            .WithPayload(payload)
            .WithQualityOfServiceLevel((MQTTnet.Protocol.MqttQualityOfServiceLevel)qos)
            .WithRetainFlag(retainFlag)
            .Build());
        /// <summary>
        /// Subscribe topic.
        /// </summary>
        /// <param name="topic">Topic.</param>
        /// <param name="qos">Quality of Service.</param>
        /// <returns>Task.</returns>
        public static async Task SubscribeAsync(string topic, int qos = 1) =>
          await client.SubscribeAsync(new TopicFilterBuilder()
            .WithTopic(topic)
            .WithQualityOfServiceLevel((MQTTnet.Protocol.MqttQualityOfServiceLevel)qos)
            .Build());
    }
}
