#! /usr/bin/env ruby

class CounterServer
    @counter = Hash.new 0

    def self.increase(ids=[]) 
        if ids.class.name == "Array"
            ids.each do |id|
                @counter[id] += 1
            end
        elsif ids.class.name == "Hash"
            ids.each do |id, value|
                @counter[id] += value.to_i
            end
        end
    end
    
    def self.refresh
        counter_old = @counter
        @counter = Hash.new 0

        return counter_old
    end
end
