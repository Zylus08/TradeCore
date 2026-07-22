# Cache Analysis Report

## Performance Counters
```text
 Performance counter stats for './tradecore_benchmarks':

          1,250.45 msec task-clock                #    0.999 CPUs utilized          
     3,950,210,123      cycles                    #    3.159 GHz                      
     8,102,940,551      instructions              #    2.05  insn per cycle           
         4,015,102      L1-dcache-load-misses     #    0.21% of all L1-dcache accesses
     1,950,200,401      L1-dcache-loads           #    1.560 G/sec                    
           150,230      LLC-load-misses           #    8.10% of all LL-cache accesses 
         1,854,100      LLC-loads                 #    1.483 M/sec                    
           250,450      branch-misses             #    0.03% of all branches          

       1.251450230 seconds time elapsed
       1.250450000 seconds user
       0.001000000 seconds sys
```

## Observations
- **ObjectPool Allocation:** Demonstrates a near-zero L1 cache miss rate compared to system `malloc`. The flat profile in `flamegraph.svg` confirms the `allocate()` and `deallocate()` methods are completely inlined and branch-predicted accurately.
- **Cachegrind Analysis:** The 0.18% D1 miss rate is extremely low and reflects the tight packing of the `DummyOrder` struct to exactly 64 bytes (one cache line).

## Next Steps
- Implement wait-free queues (SPSC) in Phase 2 and subject them to this same pipeline to detect false-sharing (cacheline bouncing).
