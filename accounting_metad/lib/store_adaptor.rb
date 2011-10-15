#!/usr/bin/env ruby
require 'logger'
require File.expand_path("yaml_loader.rb", File.dirname(__FILE__))
require File.expand_path("mrrd.rb", File.dirname(__FILE__))
require File.expand_path("channel_info.rb", File.dirname(__FILE__))

class StoreAdaptor
    def self.append(ip, info)
        raise "virtual function called"
    end
end

class LogStoreAdaptor < StoreAdaptor
    @output_config = YLoader.get(File.expand_path("../includes/settings.yml", File.dirname(__FILE__)))["output"]

    def self.append(ip, info)
        logdir = File.expand_path("#{ip}/", @output_config["logs"])
        unless File.directory?(logdir)
            Dir.mkdir(logdir) 
        end
        logfile = File.expand_path("channel_#{info[:accounting_id]}.log", logdir)

        Logger.new(logfile).info(info.inspect)
    end
end

class RRDStoreAdaptor < StoreAdaptor
    @output_config = YLoader.get(File.expand_path("../includes/settings.yml", File.dirname(__FILE__)))["output"]

    # target: :host@:id@:target
    # value:  integer
    def self.append(target, value)
        targetfile = File.expand_path(target.gsub("@", "/") + ".rrd", @output_config["rrds"])
        target_s = target.split("@", 4)
        target_s[2] = ChannelInfo.getNameByID(target_s[2])
        targetfile_s = File.expand_path(target_s.join("/") + ".rrd", @output_config["rrds"])

        if (File.exists?(targetfile))
            MRRD.update(targetfile, value)
        else
            MRRD.update(targetfile_s, value)
        end
    end

end

if __FILE__ == $0
    [0, 104, 200, 201, "fmn026_035", "status"].each do |accounting_id|
        RRDStoreAdaptor.append("test@__SUMMARY__@#{accounting_id}@requests", 900)
        RRDStoreAdaptor.append("test@__SUMMARY__@#{accounting_id}@bytes_out", 1200)
    end
end
