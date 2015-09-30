import sys,os
import numpy 

avail_threads = [4,8,16,32,48,64]
#avail_threads = [1,2,4,8,16]
#avail_threads = [64]

run_name = sys.argv[1]
outputs_folder = sys.argv[2]

files = [os.path.join(outputs_folder,f) for f in os.listdir(outputs_folder) if f.startswith(run_name+"-") and f.endswith("data") and len(f.split(run_name)[1].split("-"))==3 and f.split(run_name)[1].split("-")[0]==""]

runs = {}
for f in files:
	try:
		threads = int(os.path.basename(f).split(run_name)[1].split("-")[1])
	except:
		sys.stderr.write(f+"\n")
		continue
	if threads not in runs:
		runs[threads] = {"time":[],"commits":[],"aborts":[],"power":[],"power_t":[],"retries":[],"edp":[],"edp_t":[]}
	f = open(f)
	power = []
	power_t = -1
	retries = -1
	time = -1
	commits = -1
	aborts = -1
	for line in f:
		line = line.strip().lower()
		if "power" in line and "total" not in line:
			power.append(float(line.split(" ")[-1]))
		#if "total enrgy" in line:
		#	power_t = float(line.split(" ")[-1])
		if "time" in line:
			if "sec" in line or "seconds" in line:
				time = float(line.split(" ")[-2])
			else:
	                        time = float(line.split(" ")[-1])
		if "commits" in line:
			commits = int(line.split(" ")[-1])
		if "aborts" in line:
			aborts = int(line.split(" ")[-1])
		if "retries" in line:
                        retries = int(line.split(" ")[-1])
	f.close()
	affectedpower = 0
	if retries == -1 or time == -1 or commits == -1 or aborts == -1 or len(power) == 0:
		continue
	else:
		#runs[threads]["power"].append(power)
		#runs[threads]["power_t"].append(power_t)
		runs[threads]["time"].append(time)
		runs[threads]["commits"].append(commits)
		runs[threads]["aborts"].append(aborts)
		runs[threads]["retries"].append(retries)
		if threads <= 16:
			affectedpower = power[3]
		elif threads <=32:
			affectedpower = power[3] + power[0]
		elif threads <= 48:
			affectedpower = power[0] + power[1]+ power[-3]
		else:
			affectedpower = power[-1] + power[-2] + power[-3] + power[-4]
		runs[threads]["power"].append(affectedpower)
		runs[threads]["power_t"].append(power[-1] + power[-2] + power[-3] + power[-4])
		runs[threads]["edp"].append(affectedpower*time)
		runs[threads]["edp_t"].append(runs[threads]["power_t"][-1]*time)	
	

#runs = [(k,v) for (k,v) in runs.items()]
#runs.sort()
print "threads\tcommits\taborts\tratio\tretries\tpower\tpower_t\ttime\tEDP\tEDP_t\tpower_std\tpowert_std\ttime_std\tedp_std\tedpt_std"
for k in avail_threads:
	if k not in runs:
		print str(k)+"\t0\t0\t0\t0\t0\t0\t0\t0\t0"
		continue
	v = runs[k]	
	#print "=================="
	commits_avg = numpy.average(v["commits"])
	out = str(k)+"\t"+str(commits_avg)
	aborts_avg = numpy.average(v["aborts"])
	out += "\t"+str(aborts_avg)
	out += "\t"+str(aborts_avg/(aborts_avg+commits_avg))
	retries_avg = numpy.average(v["retries"])
        out += "\t"+str(retries_avg)
	if len(v["power"]) > 5:
		power_avg = numpy.average(v["power"])
	else:
		power_avg = numpy.median(v["power"])
	power_std =  numpy.std(v["power"])
        if power_std/power_avg * 100 > 10:
                sys.stderr.write(str(k) + ", power, " + str(power_std/power_avg) + "\n")
	out += "\t"+str(power_avg)
	if len(v["power_t"]) > 5:
		powert_avg = numpy.average(v["power_t"])
	else:
		powert_avg = numpy.median(v["power_t"])
        power_t_std =  numpy.std(v["power_t"])
	if power_t_std/powert_avg * 100 > 10:
                sys.stderr.write(str(k) + ", power_t, " + str(power_t_std/powert_avg) + "\n")
	out += "\t"+str(powert_avg)
	if len(v["time"]) > 5:
		time_avg = numpy.average(v["time"])
	else:
		time_avg = numpy.median(v["time"])
	time_std =  numpy.std(v["time"])
	if time_std/time_avg * 100 > 10:
		sys.stderr.write(str(k) + ", time, " + str(time_std/time_avg) + "\n")
        if len(v["edp"]) > 5:
                edp_avg = numpy.average(v["edp"])
        else:
                edp_avg = numpy.median(v["edp"])
        edp_std =  numpy.std(v["edp"])
        if edp_std/edp_avg * 100 > 10:
                sys.stderr.write(str(k) + ", edp, " + str(edp_std/edp_avg) + "\n")
        if len(v["edp_t"]) > 5:
                edpt_avg = numpy.average(v["edp_t"])
        else:
                edpt_avg = numpy.median(v["edp_t"])
        edpt_std =  numpy.std(v["edp_t"])
        if edpt_std/edpt_avg * 100 > 10:
                sys.stderr.write(str(k) + ", edp_t, " + str(edpt_std/edpt_avg) + "\n")
	out += "\t"+str(time_avg)
	out += "\t"+str(edp_avg)
	out += "\t"+str(edpt_avg)
        out += "\t"+str(power_std)
        out += "\t"+str(power_t_std)
        out += "\t"+str(time_std)
	out += "\t"+str(edp_std)
	out += "\t"+str(edpt_std)
	print out
	#print str(k)+"\t"+"\t".join([str(numpy.average(value)) for metric,value in v.items()])
	#print "__________________"
	#for metric, value in v.items():
	#	print metric +"\t" + str(value)
