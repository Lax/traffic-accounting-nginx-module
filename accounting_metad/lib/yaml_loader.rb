#!/usr/bin/env ruby
require 'yaml'

class YLoader
	def self.get(filename)
		puts "YLoader: #{filename}"
		File.exists?(filename) ? YAML.load_file(filename) : YAML.load_file(filename + '.default')
	end

end

if __FILE__ == $0
	p YLoader.get("includes/settings.yml")
	p YAML.load_file("includes/settings.yml")
end
