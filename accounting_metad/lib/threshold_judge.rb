#! /usr/bin/env ruby
#require 'send_nsca'
require File.expand_path("yaml_loader.rb", File.dirname(__FILE__))

class ThresholdJudge
	@@thresholds = YLoader.get(File.expand_path("../includes/thresholds.yml", File.dirname(__FILE__)))

	def self.thresholds
		@@thresholds
	end

	def self.append(name, value, debug=false)
		if @@thresholds.include?(name)
			short_name = name.gsub('__SUMMARY__', 'S').gsub('@', '_')
			puts "\t#{name}(#{short_name}) => #{value}"
			#return_code = self.assert_normal?(name, value) ? SendNsca::STATUS_OK : SendNsca::STATUS_WARNING
			#args = {
			#	:nscahost => "nagios-server-name",
			#	:port => 5667,
			#	:hostname => "traffic-server-name",
			#	:service => short_name,
			#	:return_code => return_code,
			#	:status => "OK - #{short_name}: #{value}|#{short_name}_count = #{value}r" 
			#}
			#SendNsca::NscaConnection.new(args).send_nsca if debug
			sleep 0.2 if debug
		end
	end

	def self.value_exceeded?(name, value, debug=false)
		puts "[#{debug}]ve: #{name} #{value}" if debug
		return true if value > @@thresholds[name]
		return false
	end

	def self.assert_normal?(name, value, debug=false)
		puts "[#{debug}]an: #{name} #{value}" if debug
		return false if self.value_exceeded?(name, value, debug)
		return true
	end
end

if __FILE__ == $0
	t = { :hello => 40, :hi => 100 }
	t.each do |k, v|
		puts "#{k} => #{v} in normal level." if ThresholdJudge.assert_normal?(k, v, true)
		puts "#{k} => #{v} in normal level." if ThresholdJudge.assert_normal?(k, v)
	end

	ThresholdJudge.append("__SUMMARY__@__SUMMARY__@__SUMMARY__@502", rand(1000), true)
	10.times do
		ThresholdJudge.append("__SUMMARY__@__SUMMARY__@__SUMMARY__@requests", rand(10), true)
	end
end
