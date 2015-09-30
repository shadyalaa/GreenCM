# Algorithm for Green-CM

*Green-CM: Energy efficient contention management for Transactional Memory*

Transactional memory (TM) is emerging as an attractive synchronization mechanism for concurrent computing. In this work we aim at filling a relevant gap in the area of TM, by investigating the issue of energy efficiency for one crucial building block of TM systems: contention management.

Green-CM, the solution proposed here, is the first contention management scheme explicitly designed to jointly optimize both performance and energy consumption. To this end Green-CM combines three key mechanisms: 
* it leverages on a novel asymmetric design, which combines different back- off policies in order to take advantage of dynamic frequency and voltage scaling; 
* it introduces an energy efficient design of the back-off mechanism, which combines spin-based and sleep-based implementations;
* it makes extensive use of self- tuning mechanisms to pursue optimal efficiency across highly heterogeneous workloads

The folder **GreenCM**, contains the implementaton of the algorithm with TinySTM. It also has the following benchmarks:
*STAMP: 
*STMbench7:
*Memcached:

For questions, contact the author: Shady Alaa - salaa@gsd.inesc-id.pt

When using this work, please cite accordingly: Shady Issa, Paolo Romano and Mats Brorsson, "Green-CM: Energy efficient contention management for Transactional Memory", Proceedings of the International Conference on Parallel Processing, ICPP 2015
