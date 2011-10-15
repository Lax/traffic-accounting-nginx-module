#! /usr/bin/env ruby

#  accounting_metad.rb
#  Meta server for ngx_http_accounting_module
#  Lantao Liu ( liulantao@gmail.com )

require 'lib/accounting_metad.rb'
require 'lib/yaml_loader.rb'

settings = YLoader.get(File.expand_path("includes/settings.yml", File.dirname(__FILE__)))

AccountingMetad.new(settings).start
