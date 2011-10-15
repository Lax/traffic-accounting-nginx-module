#! /usr/bin/env ruby
require File.expand_path("yaml_loader.rb", File.dirname(__FILE__))

class ServerInfo
    @@serverinfo = YLoader.get(File.expand_path("../includes/server_info.yml", File.dirname(__FILE__)))
    
    def self.groupname(ip)
        @@serverinfo[ip] || "unspecified_grid"
    end
end

if __FILE__ == $0
    ["10.22.202.10", "10.31.148.111", "10.7.16.16", "127.0.0.1"].each do |ip|
        puts "#{ip} :\t" + ServerInfo.groupname(ip)
    end
end
