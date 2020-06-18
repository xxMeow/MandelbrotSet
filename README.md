# Mandelbrot Set in 3 Ways (feat. MPI)

![Mandelbrot](Images/Mandelbrot.bmp)

### Abstract

3 ways to generate 400x400 BMP image of mandelbrot set.



### Execution & Sample Results

> `-n` specifies the number of processes to be used in Static and Dynamic methods, it should be >= 2 since there's a master.

##### Sequential Method

```bash
> Sequential.exe
```

<img src="Images/sequential.jpg" alt="sequential" style="zoom: 33%;" />

##### Static Method with MPI

```bash
> mpiexe -n 4 Static.exe
```

<img src="Images/dynamic.jpg" alt="dynamic" style="zoom: 33%;" />

##### Dynamic Method with MPI

```bash
> mpiexe -n 9 Dynamic.exe
```

<img src="Images/static.jpg" alt="static" style="zoom: 33%;" />

