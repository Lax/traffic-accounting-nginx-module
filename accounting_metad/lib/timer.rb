# Ruby license. Copyright (C)2004-2008 Joel VanderWerf.
# Contact mailto:vjoel@users.sourceforge.net.
#
# A lightweight, non-drifting, self-correcting timer. Average error is bounded
# as long as, on average, there is enough time to complete the work done, and
# the timer is checked often enough. It is lightweight in the sense that no
# threads are created. Can be used either as an internal iterator (Timer.every)
# or as an external iterator (Timer.new). Obviously, the GC can cause a
# temporary slippage.
#
# Simple usage:
#
#   require 'timer'
#
#   Timer.every(0.1, 0.5) { |elapsed| puts elapsed }
#
#   timer = Timer.new(0.1)
#   5.times do
#     puts timer.elapsed
#     timer.wait
#   end 
#
class Timer
  # Yields to the supplied block every +period+ seconds. The value yielded is
  # the total elapsed time (an instance of +Time+). If +expire+ is given, then
  # #every returns after that amount of elapsed time.
  def Timer.every(period, expire = nil)
    target = time_start = Time.now
    loop do
      elapsed = Time.now - time_start
      break if expire and elapsed > expire
      yield elapsed
      target += period
      error = target - Time.now
      sleep error if error > 0
    end
  end
  
  # Make a Timer that can be checked when needed, using #wait or #if_ready. The
  # advantage over Timer.every is that the timer can be checked on separate
  # passes through a loop.
  def initialize(period = 60)
    @period = period
    restart
  end
  
  attr_accessor :period
  
  # Call this to restart the timer after a period of inactivity (e.g., the user
  # hits the pause button, and then hits the go button).
  def restart
    @target = @time_start = Time.now
  end
  
  # Time on timer since instantiation or last #restart.
  def elapsed
    Time.now - @time_start
  end
  
  # Wait for the next cycle, if time remains in the current cycle. Otherwise,
  # return immediately to caller.
  def wait(per = nil)
    @target += per || @period
    error = @target - Time.now
    sleep error if error > 0
    true
  end
  
  # Yield to the block if no time remains in cycle. Otherwise, return
  # immediately to caller
  def if_ready
    error = @target + @period - Time.now
    if error <= 0
      @target += @period
      elapsed = Time.now - @time_start
      yield elapsed
    end
  end
end

if __FILE__ == $0

  require 'test/unit'
  
  # These tests may not work on a slow machine or heavily loaded system; try
  # adjusting FUDGE.
  class Test_Timer < Test::Unit::TestCase # :nodoc:

    STEPS   = 100
    PERIOD  = 0.01
    FUDGE   = 0.01 # a constant independent of period.

    def generic_test(steps = STEPS, period = PERIOD, fudge = FUDGE)
      max_sleep = period/2
      
      start_time = Time.now
      yield steps, period, max_sleep
      finish_time = Time.now
      
      assert_in_delta(
        start_time.to_f + steps*period,
        finish_time.to_f,
        period + fudge,
        "delta = #{finish_time.to_f - (start_time.to_f + steps*period)}"
      )
    end

    def test_every
      generic_test do |steps, period, max_sleep|
        Timer.every period do |elapsed|
          s = rand()*max_sleep
          #puts "#{elapsed} elapsed; sleeping #{s}"
          sleep(s)
          steps -= 1
          break if steps == 0
        end
      end
    end
    
    def test_every_with_expire
      generic_test do |steps, period, max_sleep|
        Timer.every period, period*steps do
          s = rand()*max_sleep
          #puts "#{elapsed} elapsed; sleeping #{s}"
          sleep(s)
        end
      end
    end
    
    def test_wait
      generic_test do |steps, period, max_sleep|
        timer = Timer.new(period)
        steps.times do
          s = rand()*max_sleep
          #puts "#{timer.elapsed} elapsed; sleeping #{s}"
          sleep(s)
          timer.wait
        end
      end
    end
  
    def test_wait_per
      offset = PERIOD / 2
      generic_test(STEPS, PERIOD, FUDGE + offset) do |steps, period, max_sleep|
        timer = Timer.new(period)
        steps.times do |i|
          s = rand()*max_sleep
          #puts "#{timer.elapsed} elapsed; sleeping #{s}"
          sleep(s)
          timer.wait(i%2==0 ? period + offset : period - offset)
        end
      end
    end
  
    def test_if_ready
      generic_test do |steps, period, max_sleep|
        timer = Timer.new(period)
        catch :done do
          loop do
            timer.if_ready do |elapsed|
              s = rand()*max_sleep
              #puts "#{elapsed} elapsed; sleeping #{s}"
              sleep(s)
              steps -= 1
              throw :done if steps == 0
            end
          end
        end
      end
    end
  
  end
  
end
