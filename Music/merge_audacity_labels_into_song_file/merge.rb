label_file="labels.txt"
#song_file="in.csv"
song_file="4min_blank.csv"
round_to=20  # Inverse of song resolution (1/X of a second)

audacity_info=File.readlines(label_file)

list_of_cleansed_lines = []


# Suck the data in from the audacity file
# and make a list arrays where each entry is [float_time,label_description]
audacity_info.each do |line|

	# Round to nearest slice time
	integer_part_of_time = line.split("\t")[0].to_i
	decimal_part_of_time = (line.split("\t")[0].to_f - line.split("\t")[0].to_i)
	num_slices_in_decimal_part = (decimal_part_of_time / (1.0/round_to)).to_i
	
	# If there is more than half a slice left, add it in
	num_slices_in_decimal_part +=1 if (decimal_part_of_time-num_slices_in_decimal_part*(1.0/round_to)) > ((0.5/round_to))
	
	final = integer_part_of_time.to_f + num_slices_in_decimal_part*(1.0/round_to)

	#print line.split("\t")[0] + " -> "
	list_of_cleansed_lines.push([final,line.split("\t")[2]])
	# "%.2f ", final
	#puts line.split("\t")[2]

end

=begin
list_of_cleansed_lines.each do |line|
	printf "%.2f ", line[0]
	puts line[1]
end
=end

# Open output file which may already have data in it.
# This should find any line of that file that doesn't already
# have data in it and add the data (merge)
# If a line in the csv file already has something in it, it will be skipped
output_file=File.read(song_file)

# 
# Now open the 
file_to_write=""
output_file.each_line do |line|
	
	# If this line already has some info in it, skip it
	if line.strip.split(":").length > 1
		file_to_write+=line  # Just save it as is
		next
	else # So this line doesn't have anything but a timestamp in it yet...
		# Let's see if list_of_cleansed_lines contains a line that has the same timestamp
		merge=false
		list_of_cleansed_lines.each do |cleaned_line|
		
			#puts "Comparing " + (cleaned_line[0] * 100).to_i.to_s + " to " + (line.split(":")[0].to_f*100).to_i.to_s
		
			if (cleaned_line[0] * 100).to_i == (line.split(":")[0].to_f*100).to_i		# Do we have a merge to do?
				#puts line.strip.split(":")[0] + " looks like " + cleaned_line[0].to_s
				
				
				#puts "Found a merge"
				file_to_write += line.strip.split(":")[0] + ":MISC-PULSE #" + cleaned_line[1]
				merge=true
			end
		end
		
		if merge==false
			# Guess no merge, just save as is
			file_to_write+=line  # Just save it as is
		end
	end
end

File.open("out-" + Time.now.to_s.gsub(" ","").gsub(":","-") + ".csv", 'w') { |file| file.write(file_to_write) }

	