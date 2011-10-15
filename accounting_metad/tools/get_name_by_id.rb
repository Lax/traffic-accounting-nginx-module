#!/usr/bin/env ruby
require File.expand_path("../lib/channel_info.rb", File.dirname(__FILE__))

if __FILE__ == $0
    puts ChannelInfo.getNameByID ARGV[0]
end
