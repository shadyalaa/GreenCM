import os,sys
import numpy

resultsDir = sys.argv[1]
benchmark = sys.argv[2]
outputsDir = sys.argv[3]

avail_threads = [32,48,64]

freqs = [1.4,1.6,1.8,2.0,2.2,2.4,3.0]
configs = ["adpt","asym-adpt","dasym-adpt"]

runs = {}

for config in configs:
    runName = benchmark+"-"+config
    files = [f for f in os.listdir(resultsDir) if f.startswith(runName) and f.endswith("out")] 
    for f in files:
	threads = int(f.split(runName)[1].split("-")[1])
	if threads not in runs:
		runs[threads] = {}
		for c in configs:
			runs[threads][c] = {"freq":{}}
	data = open(os.path.join(resultsDir,f)).readlines()
	i = 0
	for line in data:
		if line.startswith("cpu "):
			if i >= threads:
				break
			i += 1
			line = line.strip().split("\t")
			freqdist = []
			for j in range(1,len(line)):
				freqdist.append(int(line[j]))
			if i not in runs[threads][config]["freq"]:
				runs[threads][config]["freq"][i] = []
			runs[threads][config]["freq"][i].append(numpy.dot(freqdist,freqs)/numpy.sum(freqdist))

runs = [(k,v) for (k,v) in runs.items()]
runs.sort()

#out = "threads\t2.7GHz\t2.4GHz\t2.2GHz\t2.0GHz\t1.8GHz\t1.6GHz\t1.4GHz\n"
#out = "threads\tp0\tp1\tp2\tp3\tp4\tp5\tp6\n"
for (t,v) in runs:
	output = open(os.path.join(outputsDir,benchmark+"-"+str(t)),"w")
	output.write("benchmark\tthreads\tp0\tp1\tp2\tp3\tp4\tp5\tp6\n")
	#out += str(t) + "\t"
	#for (k,b) in v.items():
	for k in configs:
	    b = v[k]
	    output.write(str(k.split("-")[0]) + "\t" + str(t) + "\t")
	    for c,a in b.items():
		    if c == "freq":
			afreqs = []
			p0,p1,p2,p3,p4,p5,p6=0,0,0,0,0,0,0
			for freq,occ in a.items():
				afreqs.append(numpy.average(occ))
			afreqs.sort(reverse=True)
			for afreq in afreqs:
				if afreq > 2.5:
					p0+=1
				elif afreq > 2.3:
					p1+=1
				elif afreq > 2.0:
					p2+=1
				elif afreq > 1.8:
					p3+=1
				elif afreq >1.6:
					p4+=1
				elif afreq > 1.4:
					p5+=1
				else:
					p6+=1
			output.write(str(p0)+"\t"+str(p1)+"\t"+str(p2)+"\t"+str(p3)+"\t"+str(p4)+"\t"+str(p5)+"\t"+str(p6))
				#out += str(numpy.average(afreq))+"\t"
		    output.write("\n")

	output.close()	
