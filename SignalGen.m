clc;
clear all;
% total count for generation
N=2000000;
% amplitude of the signal
A=4096;
% absolute frequenc in Hz
F=600;
% sample frequncy in Hz
Fd=1000000;
t=(0:N-1)/Fd;
S=A*sin(1*pi+2*pi*F*t)
fid=fopen('Data.bin', 'w');
fwrite(fid,S,'int16');
fclose(fid);