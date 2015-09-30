# Algorithm for Green-CM

*Green-CM: Energy efficient contention management for Transactional Memory*


Transactional memory (TM) is emerging as an attractive synchronization mechanism for concurrent comput- ing. In this work we aim at filling a relevant gap in the TM literature, by investigating the issue of energy efficiency for one crucial building block of TM systems: contention management.
Green-CM, the solution proposed in this paper, is the first contention management scheme explicitly designed to jointly optimize both performance and energy consumption. To this end Green-TM combines three key mechanisms: i) it leverages on a novel asymmetric design, which combines different back- off policies in order to take advantage of dynamic frequency and voltage scaling; ii) it introduces an energy efficient design of the back-off mechanism, which combines spin-based and sleep-based implementations; iii) it makes extensive use of self- tuning mechanisms to pursue optimal efficiency across highly heterogeneous workloads.
We evaluate Green-CM from both the energy and perfor- mance perspectives, and show that it can achieve enhanced efficiency by up to 2.35 times with respect to state of the art contention managers, with an average gain of more than 60% when using 64 threads.