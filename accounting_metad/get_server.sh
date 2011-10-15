#! /bin/sh

cd `dirname $0`
screen -r accounting_metad.rb || screen -S accounting_metad.rb ruby accounting_metad.rb 
