#! /usr/bin/env ruby
require File.expand_path("yaml_loader.rb", File.dirname(__FILE__))

class ChannelInfo
    @@channelinfo = YLoader.get(File.expand_path("../includes/channels.list", File.dirname(__FILE__)))

    def self.getNameByID(accounting_id)
        if(accounting_id =~ /^\d+$/ or accounting_id.class.name == "Fixnum")
            @@channelinfo[accounting_id.to_i] ? @@channelinfo[accounting_id.to_i].split(":")[0] : accounting_id
        else
            accounting_id
        end
    end
end

if __FILE__ == $0
    [1, 112, 201, "201", "1024", "fmn_other", "status"].each do |accounting_id|
        puts "channel #{accounting_id}.#{accounting_id.class}:\t" + ChannelInfo.getNameByID(accounting_id)
    end
end
