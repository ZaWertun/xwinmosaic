#!/usr/bin/env ruby

# This script can convert GPL (GIMP Palette) file to XWinmosaic colors file.

require 'pp'

FILE = ARGV.first
unless FILE
    STDERR.puts "Usage: #{$0} file.colors|file.gpl|-"
    STDERR.puts "  Read strings from stdin if file is `-`"
    STDERR.puts
    STDERR.puts "Example: ./gpl_to_colors.rb /usr/share/config/colors/Oxygen.colors > ~/.config/xwinmosaic/colors"
    exit 1
end

fd = (FILE == '-') ? STDIN : File.open(FILE)
colors = []
fd.readlines.each do |line|
    next if line =~ /^GIMP Palette|^Name: |^#/
    r, g, b, name = line.rstrip.split(' ', 4)
    hex = '#' + [r, g, b].map {|x| x.to_i.to_s(16).rjust(2, '0')}.join
    colors << [hex, name]
end

# colors.shuffle! # Randomize

puts '[colors]'
puts '# names = ' + colors.map{|x| x.last}.join('; ')
puts 'fallback = ' + colors.map{|x| x.first}.join('; ')
