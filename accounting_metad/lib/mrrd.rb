#! /usr/bin/env ruby
require "RRD"
require 'fileutils'

class MRRD
    def self.update(rrdfile, value)
        create rrdfile unless File.exists? rrdfile
        RRD.update(rrdfile, "#{Time.now.to_i}:#{value}")
    end

    def self.create(rrdfile)
        puts "\t==>\tCreating #{rrdfile}"

        FileUtils.mkdir_p(File.dirname rrdfile) unless File.directory?(File.dirname rrdfile)
        RRD.create(
            rrdfile,
            "--start", "0",
            "--step", "60",
            "DS:value:ABSOLUTE:120:U:U",
            "RRA:AVERAGE:0.5:1:1800",   # 1min * 60 * 30 = 1d+6h
            "RRA:AVERAGE:0.5:5:8640",   # 5min * 12 * 24 * 30 = 30d
            "RRA:AVERAGE:0.5:60:2160",  # 60min * 24 * 30 * 12 = 1y
            "RRA:AVERAGE:0.5:480:5475"  # 480min * 3 * 365 * 5 = 5y
        )
    end
end
