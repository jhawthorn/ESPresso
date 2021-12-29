require "webrick"
require "socket"
require "mqtt"
require "uri"

FILE = ".pio/build/esp32dev/firmware.bin"

class FirmwareServer < WEBrick::HTTPServlet::AbstractServlet
  def do_GET request, response
    response.status = 200
    response['Content-Type'] = "application/octet-stream"
    response.body = File.read(FILE)

    # Request a shutdown, will still finish serving the current request
    @server.shutdown
  end
end

unless File.exist?(FILE)
  raise "File not found: #{FILE}"
end

PORT = 3333
server = WEBrick::HTTPServer.new(:Port => PORT)
server.mount "/firmware.bin", FirmwareServer

def detect_address
  address = Socket.ip_address_list.select(&:ipv4?).reject(&:ipv4_loopback?).first
  address.ip_address
end

address = ENV.fetch("ADDRESS") { detect_address }

mqtt_address = ENV.fetch("MQTT_ADDRESS")
mqtt_client = MQTT::Client.connect(mqtt_address)

Thread.new do
  while server.status != :Running
    sleep 0.1
  end

  ota_url = "http://#{address}:#{PORT}/firmware.bin"
  ota_topic = "home/espresso/update_url"

  puts "Sending OTA URL: #{ota_url}"
  puts "       to topic: #{ota_topic}"

  mqtt_client.publish(ota_topic, ota_url)
end

# Enable shutdown on C-c
trap("INT"){ server.shutdown }
server.start
