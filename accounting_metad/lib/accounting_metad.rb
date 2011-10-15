#! /usr/bin/env ruby
require 'socket'
require File.expand_path('store_adaptor.rb', File.dirname(__FILE__))
require File.expand_path('counter_server.rb', File.dirname(__FILE__))
require File.expand_path('threshold_judge.rb', File.dirname(__FILE__))
require File.expand_path("server_info.rb", File.dirname(__FILE__))
require File.expand_path("channel_info.rb", File.dirname(__FILE__))
require File.expand_path("timer.rb", File.dirname(__FILE__))

class AccountingMetad
    @@merge_interval = 60

    def initialize(config)
        load_config config
        start_server
        process_packets
    end
    
    # load config and prepare
    def load_config(config)
        @input_config = config["input"]
        @output_config = config["output"]
        @@merge_interval = @input_config['merge_interval']
    end

    def start_server
        @udpsocket = UDPSocket.new
        @udpsocket.bind(@input_config["listen"]["ip"], @input_config["listen"]["port"])
    end

    def process_packets
        Thread.new do
            Timer.every @@merge_interval do
                puts Time.now
                CounterServer.refresh.sort.each do |target_key,target_value|
                    RRDStoreAdaptor.append(target_key, target_value)
                    ThresholdJudge.append(target_key, target_value, true) if ThresholdJudge.thresholds.include?(target_key)
                end
            end
        end

        loop do
            Thread.start(@udpsocket.recvfrom(1024)) do |packet|
                (logline, (prot, port, remotename, remoteip)) = packet
                time, data = logline.split("|| ", 2)
                server_groupname = ServerInfo.groupname(remoteip)

                packetinfo = Hash.new '0'
                data.chop.scan(/([^:|]+):([^:|]+)/).each do |k,v|
                    packetinfo[k.to_sym] = v
                end
                accounting_id = ChannelInfo.getNameByID(packetinfo[:accounting_id])

                [:"502", :"500", :"504", :"499", :"408", :"404", :"403", :"400", :requests, :bytes_out].each do |vvv|
                    if packetinfo[vvv].to_i > 0
                        increaser = {
                            "#{server_groupname}@#{remoteip}@__SUMMARY__@#{vvv}" => packetinfo[vvv], 
                            "#{server_groupname}@__SUMMARY__@#{accounting_id}@#{vvv}" => packetinfo[vvv],
                            "#{server_groupname}@__SUMMARY__@__SUMMARY__@#{vvv}" => packetinfo[vvv],
                            "__SUMMARY__@__SUMMARY__@#{accounting_id}@#{vvv}" => packetinfo[vvv],
                            "__SUMMARY__@__SUMMARY__@__SUMMARY__@#{vvv}" => packetinfo[vvv],
                        }
                    CounterServer.increase increaser
                    end
                end
                LogStoreAdaptor.append(remoteip, packetinfo)

                packet.close
            end
        end
    end
end
