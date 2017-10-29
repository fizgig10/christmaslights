# & 'C:\Program Files (x86)\VideoLAN\VLC\vlc.exe' -I rc --rc-host localhost:9999 ; sleep(1); ruby .\main.rb groundzero 0


require 'rubygems'  
require 'serialport'
require 'vlc-client' 		# http://www.rubydoc.info/github/mguinada/vlc-client/VLC/Client/MediaControls
require 'os'  # Detect which OS we are using...
#require 'win32ole'
#player = WIN32OLE.new('WMPlayer.OCX')

baud_rate=57600
comm_port = 2
$lag_time = -0.06  # ms to wait extra from time value in song (positive = lag)
$resolution_in_ms=50
$line = "" # Global variable that a thread will access
$list_of_colors = ["RED","ORANGE","YELLOW","GREEN","BLUE","PURPLE"]



def return_hue_value(color)
	color.upcase!
	return 0 if color=="RED"
	return 41 if color=="ORANGE"
	return 58 if color=="YELLOW"
	return 100 if color=="GREEN"
	return 252 if color=="BLUE"
	return 294 if color=="PURPLE"

	return 0 # Give red if no match

end


############
# Function #
################
# usuage_error #
##############################
# If arguments are not valid #
##############################
def usuage_error()
	puts "Usuage: ruby main.rb <SONGNAME> <HEAD_START_IN_ms>"
	puts "ex: ruby main.rb groundzero 900 0"
	puts "Or... ruby main.rb template name_of_file num_minutes"
	exit
end


##################
# Template creator
##################
def make_template_file(name_of_file,minutes)
	puts "Making file '" + name_of_file + "' " + minutes.to_s + " minutes long"
	ms=0
	outfile=File.open(name_of_file, "w")
	1.upto(minutes.to_i*60*(1000/$resolution_in_ms)) do
		outfile.write(sprintf('%08.2f', ms / 1000.0))
		outfile.write(":\n")
		ms += $resolution_in_ms
	end	
end



###########
# Arguments
###########

if ARGV[0]=="template"
    make_template_file(ARGV[1],ARGV[2])
	exit
end

$song=ARGV[0]
seek_to_seconds=ARGV[1].to_i



###################
# Serial Port Setup
###################

serial_parameters = {"baud" => baud_rate, "data_bits"=>8, "stop_bits"=>1, "parity"=>SerialPort::NONE }
if OS.windows?
	$serial_port = SerialPort.new(comm_port-1, serial_parameters)  # I have to subtract 1 in windows for some reason
else
	$serial_port = SerialPort.new('/dev/ttyAMA0', serial_parameters)  # I have to subtract 1 in windows for some reason
end
$serial_port.read_timeout = 150
#$serial_port.write_timeout = 150


# Read song file into song string variable
#song = File.readlines("//home//pi//lights//Music//" + $song + ".csv")
song = File.readlines("./Music/" + $song + ".csv")





def convert_to_hex_command(english_line)
	
	# 2 = Tree
	# 3 = Arch
	# 4 = Canes
	# 5 = Presents
	
	
	byte_line = ""
	
	#############
	# Broadcast #
	#############
	# ALL
	if english_line.split("-")[0]=="ALL" 
		byte_line += "\x00" # Broadcast is address 0
		
		# ALL-BLANK
		if english_line.split("-")[1]=="BLANK" 
			byte_line += "\x00" # Control Command
			byte_line += "\x01" # Blank all lights
			byte_line += "\x00" # Dummy
			byte_line += "\x00" # Dummy
			byte_line += "\x00" # Dummy
		# ALL-BRIGHTNESS-255
		elsif english_line.split("-")[1]=="BRIGHTNESS" 
			byte_line += "\x00" # Control Command
			byte_line += "\x02" # Brightness command
			byte_line += [english_line.split("-")[2].to_i].pack("C")	# Brightness value
			byte_line += "\x00" # Dummy
			byte_line += "\x00" # Dummy
		end
	end # ALL
			
	
	
	########
	# Tree #
	########
	if english_line.split("-")[0]=="TREE"  
		byte_line += "\x02" #Tree is address 2
		
		# Send two bytes directly
		# TREE-DIRECT-BINARY1-BINARY2
		if english_line.split("-")[1]=="DIRECT" 
			byte_line += "\x01" # Direct is command 1
			byte_line += [english_line.split("-")[2].to_i(2)].pack("C") # First Byte
			byte_line += [english_line.split("-")[3].to_i(2)].pack("C") # Second Byte
			byte_line += "\x00" # Dummy byte
			byte_line += "\x00" # Dummy byte
		end			
			
		
		# TREE-SPIN-NUM_TIME_SLICES-CW-CUM
		if english_line.split("-")[1]=="SPIN" 
			byte_line += "\x02" #Spin is command 2
			byte_line += [english_line.split("-")[2].to_i].pack("C")	# Time
			if english_line.split("-")[3]=="CW" 
				byte_line += "\x00"
			else 
				byte_line += "\x01"
			end
			if english_line.split("-")[4]=="CUM"
				byte_line += "\x01"
			else 
				byte_line += "\x00"
			end
			byte_line += "\x00" # Dummy byte
		end
		
	end # Tree Section
	
	
	
	##########
	# ARCHES #
	##########
	if english_line.split("-")[0]=="ARCHES"
		byte_line += "\x03" # Arches is address 3
		
		# ARCHES-BLANK
		if english_line.split("-")[1]=="BLANK"
			byte_line += "\x00" # Fill command is 0
			byte_line += "\x01" # Fill command is 0
			byte_line += "\x00" # Fill command is 0
			byte_line += "\x00" # Fill command is 0
			byte_line += "\x00" # Fill command is 0
		end
		
		# ARCHES-FILLALL-RED-GREEN-BLUE
		if english_line.split("-")[1]=="FILLALL"
			byte_line += "\x01" # Fill command is 1
			byte_line += [english_line.split("-")[2].to_i].pack("C") # Red
			byte_line += [english_line.split("-")[3].to_i].pack("C") # Green
			byte_line += [english_line.split("-")[4].to_i].pack("C") # Blue
			byte_line += "\x00" # Dummy byte
		end
		
	
		# ARCHES-FILLNUM-RED-GREEN-BLUE
		if english_line.split("-")[1]=="FILLNUM"
			byte_line += "\x02" # Fillnum command is 2
			byte_line += [english_line.split("-")[2].to_i].pack("C") # Arch Num
			byte_line += [english_line.split("-")[3].to_i].pack("C") # Red
			byte_line += [english_line.split("-")[4].to_i].pack("C") # Green
			byte_line += [english_line.split("-")[5].to_i].pack("C") # Blue
		end
		
		# ARCHES-STATIC_RAINBOW-STARTHUE
		if english_line.split("-")[1]=="STATIC_RAINBOW"
			byte_line += "\x03" # STATIC_RAINBOW command is 3
			byte_line += [english_line.split("-")[2].to_i].pack("C") # Start Hue
			byte_line += "\x00" # Dummy byte
			byte_line += "\x00" # Dummy byte
			byte_line += "\x00" # Dummy byte
		end
		
		# ARCHES-MOVING_RAINBOW-NUM_TIME_SLICES-STARTHUE-mS_BETWEEN_FRAMES-HUE_DIF_BETWEEN_UPDATES
		if english_line.split("-")[1]=="MOVING_RAINBOW"
			byte_line += "\x04" # MOVING_RAINBOW command is 4
			byte_line += [english_line.split("-")[2].to_i].pack("C") # NUM_TIME_SLICES
			byte_line += [english_line.split("-")[3].to_i].pack("C") # START_HUE
			byte_line += [english_line.split("-")[4].to_i].pack("C") # mS_BETWEEN_FRAMES
			byte_line += [english_line.split("-")[5].to_i].pack("C") # HUE_DIF_BETWEEN_UPDATES
		end
		
		# ARCHES-SPARKLE_RAINBOW-NUM_TIME_SLICES-STARTHUE-mS_BETWEEN_FRAMES-HUE_DIF_BETWEEN_UPDATES
		if english_line.split("-")[1]=="SPARKLE_RAINBOW"
			byte_line += "\x05" # SPARKLE_RAINBOW command is 5
			byte_line += [english_line.split("-")[2].to_i].pack("C") # NUM_TIME_SLICES
			byte_line += [english_line.split("-")[3].to_i].pack("C") # START_HUE
			byte_line += [english_line.split("-")[4].to_i].pack("C") # mS_BETWEEN_FRAMES
			byte_line += [english_line.split("-")[5].to_i].pack("C") # HUE_DIF_BETWEEN_UPDATES
		end
		
		# ARCHES-CYLON-NUM_TIME_SLICES-START_LED-END_LED-HUE
		if english_line.split("-")[1]=="CYLON"
			byte_line += "\x06" # CYLON command is 6
			byte_line += [english_line.split("-")[2].to_i].pack("C") # NUM_TIME_SLICES
			byte_line += [english_line.split("-")[3].to_i].pack("C") # START_LED
			byte_line += [english_line.split("-")[4].to_i].pack("C") # END_LED
			byte_line += [english_line.split("-")[5].to_i].pack("C") # HUE
		end
		
		# ARCHES-FIRE-NUM_TIME_SLICES-mS_BETWEEN_FRAMES-COOLING-SPARKING
		if english_line.split("-")[1]=="FIRE"
			byte_line += "\x07" # FIRE command is 7
			byte_line += [english_line.split("-")[2].to_i].pack("C") # NUM_TIME_SLICES
			byte_line += [english_line.split("-")[3].to_i].pack("C") # mS_BETWEEN_FRAMES
			byte_line += [english_line.split("-")[4].to_i].pack("C") # COOLING
			byte_line += [english_line.split("-")[5].to_i].pack("C") # SPARKING
		end
		
		# ARCHES-THREE_ARCH_COLORS-NUM_TIME_SLICES-HUE1-HUE2-HUE3
		if english_line.split("-")[1]=="THREE_ARCH_COLORS"
			byte_line += "\x08" # command is 8
			
			# Iterate through all three values (command[2] - command[4])
			for i in 2..4 do
				if $list_of_colors.any? {|word| english_line.split("-")[i].upcase.include?(word) } # Does the value include any color names?
					byte_line += [return_hue_value(english_line.split("-")[i].upcase)].pack("C") # Convert name to hue value then pack it.
				else	# Must be a numerical value so pack it directly
					byte_line += [english_line.split("-")[i].to_i].pack("C") # Hue1
				end		
			end
			
			byte_line += "\x00" # Dummy byte
		end
		
		# ARCHES-INCOMING-HUE_H-HUE_S-HUE_V
		if english_line.split("-")[1]=="INCOMING"
			byte_line += "\x09" # Incoming command is 9
			byte_line += [english_line.split("-")[2].to_i].pack("C") # NUM_TIME_SLICES
			byte_line += [english_line.split("-")[3].to_i].pack("C") # HUE_H
			byte_line += [english_line.split("-")[4].to_i].pack("C") # HUE_S
			byte_line += [english_line.split("-")[5].to_i].pack("C") # HUE_V
		end
		
		
		
	end # Arches

	
	#########
	# CANES #
	#########
	if english_line.split("-")[0]=="CANES"
		byte_line += "\x04" # Canes is address 4
		
		# CANES-FILLALL-RED#-GREEN#-BLUE#
		if english_line.split("-")[1]=="FILLALL"
			byte_line += "\x01" #  Fill all
			byte_line += [english_line.split("-")[2].to_i].pack("C")
			byte_line += [english_line.split("-")[3].to_i].pack("C")
			byte_line += [english_line.split("-")[4].to_i].pack("C")
			byte_line += "\x00" # Dummy byte
		end
		
		# CANES-MARQUEE-NUM_TIME_SLICES-MS_FLICKERSPEED
		if english_line.split("-")[1]=="MARQUEE"
			byte_line += "\x02" #  Marquee
			byte_line += [english_line.split("-")[2].to_i].pack("C") # Time in 10's
			byte_line += [english_line.split("-")[3].to_i].pack("C") # ms per cycle (blink speed)
			byte_line += "\x00" # Dummy byte
			byte_line += "\x00" # Dummy byte
		end
		
		# CANES-STACK-NUM_TIME_SLICES
		if english_line.split("-")[1]=="STACK"
			byte_line += "\x03" #  Stack
			byte_line += [english_line.split("-")[2].to_i].pack("C") # Time in 10's
			byte_line += "\x00" # Dummy byte
			byte_line += "\x00" # Dummy byte
			byte_line += "\x00" # Dummy byte
		end
		
		# CANES-CYLON-NUM_TIME_SLICES-STARTLED-ENDLED
		if english_line.split("-")[1]=="CYLON"
			byte_line += "\x04" #  Cylon
			byte_line += [english_line.split("-")[2].to_i].pack("C") # Time in 10's
			byte_line += [english_line.split("-")[3].to_i].pack("C") # Starting LED
			byte_line += [english_line.split("-")[4].to_i].pack("C") # Ending LED
			byte_line += "\x00" # Dummy byte
		end
		
		# CANES-MARCH-NUM_TIME_SLICES-STARTCANE-ENDCANE
		if english_line.split("-")[1]=="MARCH"
			byte_line += "\x05" # March
			byte_line += [english_line.split("-")[2].to_i].pack("C") # Time in 10's
			byte_line += [english_line.split("-")[3].to_i].pack("C") # Starting Cane
			byte_line += [english_line.split("-")[4].to_i].pack("C") # Ending Cane		
			byte_line += "\x00" # Dummy byte
		end
		
		# CANES-SWIPE-NUM_TIME_SLICES-DIR  (1=up,2=down,3=right,4=left)
		if english_line.split("-")[1]=="SWIPE"
			byte_line += "\x06" # Swipe
			byte_line += [english_line.split("-")[2].to_i].pack("C") # Time in 10's
			byte_line += [english_line.split("-")[3].to_i].pack("C") # Direction 
			byte_line += "\x00" # Dummy byte
			byte_line += "\x00" # Dummy byte
		end
		
		# CANES-CANE_RA-NUMS_OF_CANES_TO_BE_ON
		if english_line.split("-")[1]=="CANE_RA"
			byte_line += "\x07" # CANE_RA (Random Access)

			bitmask=0
			bitmask |= (1<<0) if english_line.split("-")[2].include?("0")
			bitmask |= (1<<1) if english_line.split("-")[2].include?("1")
			bitmask |= (1<<2) if english_line.split("-")[2].include?("2")
			bitmask |= (1<<3) if english_line.split("-")[2].include?("3")
			bitmask |= (1<<4) if english_line.split("-")[2].include?("4")
			bitmask |= (1<<5) if english_line.split("-")[2].include?("5")
			byte_line += [bitmask].pack("C")

			byte_line += "\x00" # Dummy byte
			byte_line += "\x00" # Dummy byte
			byte_line += "\x00" # Dummy byte
		end
				
		
	end # CANES Section
	
	
	########
	# MISC #
	########
	if english_line.split("-")[0]=="MISC"
		byte_line += "\x05" # address 5
		
		if english_line.split("-")[1]=="DIRECT" or english_line.split("-")[1]=="ADDITIVE"  # Direct access
		
			byte_line += "\x01" if english_line.split("-")[1]=="DIRECT" # command to send directly
			byte_line += "\x02" if english_line.split("-")[1]=="ADDITIVE" # command to send additive
			
			# First byte
			bitmask=0
			# MISC-P1 P2 P3 B1 B2 B3 B4 B5 GUTTERS
			
			bitmask |= (1<<0) if english_line.split("-")[2].include?("B1")
			bitmask |= (1<<1) if english_line.split("-")[2].include?("B2")
			bitmask |= (1<<2) if english_line.split("-")[2].include?("B3")
			bitmask |= (1<<3) if english_line.split("-")[2].include?("B4")
			bitmask |= (1<<4) if english_line.split("-")[2].include?("B5")
			bitmask |= (1<<5) if english_line.split("-")[2].include?("P1")
			bitmask |= (1<<6) if english_line.split("-")[2].include?("P2")
			bitmask |= (1<<7) if english_line.split("-")[2].include?("P3")
			byte_line += [bitmask].pack("C")
			
			# Second byte
			bitmask=0
			bitmask |= (1<<0) if english_line.split("-")[2].include?("GUTTERS")
			bitmask |= (1<<1) if english_line.split("-")[2].include?("PENGUIN")
			bitmask |= (1<<2) if english_line.split("-")[2].include?("BEAR")
			bitmask |= (1<<3) if english_line.split("-")[2].include?("PIG")
			byte_line += [bitmask].pack("C")
			
			byte_line += "\x00" # Dummy byte
			byte_line += "\x00" # Dummy byte
		end
		
		if english_line.split("-")[1]=="PULSE"  # Used for timing checks
			byte_line += "\x00"  # Control command
			byte_line += "\x02"  # pulse command
			byte_line += "\x00" # Dummy byte
			byte_line += "\x00" # Dummy byte
			byte_line += "\x00" # Dummy byte
		end
		
	end
	
	return byte_line

end









##########
# Thread #
#######################
# execute_line_thread #
###################################################################
# This non-blocking thread is called every $resolution_in_ms time #
# period in the main loop later.  This makes it a clean call with #
# no cummulative delay error.                                     #
###################################################################
#execute_line_thread = Thread.new() do
def play_line(line_to_play)

	current_line = line_to_play.split(":")[1]  # Get rid of the leading time

	return if current_line == "\n" or current_line.nil?
	
	current_line = current_line.split("#")[0]  # Get rid of comments if any
	current_line = current_line.strip  # Get rid of any whitespace
	current_line = current_line.upcase
	
	byte_line = ""
	
	print line_to_play.split(":")[0] + ":  "
	current_line.split("|").each do |segment|
		print segment + "    "
		byte_line += convert_to_hex_command(segment) # Convert english to hex
		#puts "Byteline currently " + byte_line.length.to_s + " chars long"
	end
	
	# Print remark if it's there
	if line_to_play.split("#").length > 1
		print "  " + line_to_play.split("#")[1] 
	else
		puts
	end
	
	# Print out each byte to the serial port
	for i in 0..byte_line.length-1 do
		$serial_port.write(byte_line[i])  # Doing it this way avoids sending the /n character at the end of any string
	end
		
end










###############
# The song loop
###############

# vlc methods: play, pause, seek
vlc = VLC::Client.new('localhost', 9999)
vlc.connect
vlc.play('R:\Matts Docs\Christmas Lights\Music\\' + ARGV[0] + '.mp3')
vlc.pause
#sleep(1) # Let it load
vlc.seek(seconds=seek_to_seconds)
vlc.pause

# Get start time as accurate float number
time_start=Time.new.to_f-seek_to_seconds

# Go through eace line and look at its timestamp
song.each do |current_line|
	
	current_time=Time.new.to_f - time_start
	
	#puts current_time.to_s
	
	# Wait until we catch up to the time of the current line
	while current_line.split(":")[0].to_f > (current_time)
		sleep(0.001) # Don't go too crazy fast...
		current_time=Time.new.to_f - time_start
	end  # Sit here...
	
	
	#print current_time.to_s + " -> "
	play_line(current_line)	
	
	
	
=begin	
	# Can't sleep for negative time...
	if ($lag_time + $resolution_in_ms/1000.0) < 0
		$lag_time = -($resolution_in_ms/1000.0) 
		puts "*** Warning, lag time adjusted to prevent negative sleep time"
	end
	sleep($resolution_in_ms/1000.0 + $lag_time)
	$lag_time=0  # This makes the lag only get used the first cycle (once)
=end
end
	
#HUE Cheatsheet
# Red=0
# Orange=41
# Yellow=58
# Green=100
# Blue=252
# Purple=294


# ALL-BLANK
# ALL-BRIGHTNESS-255

# TREE-DIRECT-BINARY1-BINARY2
# TREE-SPIN-NUM_TIME_SLICES-CW-CUM

# ARCHES-FILLALL-RED-GREEN-BLUE
# ARCHES-FILLNUM-RED-GREEN-BLUE
# ARCHES-STATIC_RAINBOW-STARTHUE
# ARCHES-MOVING_RAINBOW-NUM_TIME_SLICES-STARTHUE-mS_BETWEEN_FRAMES-HUE_DIF_BETWEEN_UPDATES
# ARCHES-SPARKLE_RAINBOW-NUM_TIME_SLICES-STARTHUE-mS_BETWEEN_FRAMES-HUE_DIF_BETWEEN_UPDATES
# ARCHES-CYLON-NUM_TIME_SLICES-START_LED-END_LED-HUE
# ARCHES-FIRE-NUM_TIME_SLICES-mS_BETWEEN_FRAMES-COOLING-SPARKING
# ARCHES-THREE_ARCH_COLORS-NUM_TIME_SLICES-HUE1-HUE2-HUE3
# ARCHES-INCOMING-HUE_H-HUE_S-HUE_V

# CANES-FILLALL-RED#-GREEN#-BLUE#
# CANES-MARQUEE-NUM_TIME_SLICES-MS_FLICKERSPEED
# CANES-STACK-NUM_TIME_SLICES
# CANES-CYLON-NUM_TIME_SLICES-STARTLED-ENDLED
# CANES-MARCH-NUM_TIME_SLICES-STARTCANE-ENDCANE
# CANES-SWIPE-NUM_TIME_SLICES-DIR  (1=up,2=down,3=right,4=left)
# CANES-CANE_RA-NUMS_OF_CANES_TO_BE_ON
