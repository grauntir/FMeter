clc;
clear all;
% total count for generation
N=100000;
% amplitude of the signal
A=4096;
% absolute frequenc in Hz
F=6;
% sample frequncy in Hz
Fd=10000;
t=(0:N-1)/Fd;
S=A*sin(1*pi+2*pi*F*t)
fid=fopen('Data.bin', 'w');
fwrite(fid,S,'int16');
fclose(fid);