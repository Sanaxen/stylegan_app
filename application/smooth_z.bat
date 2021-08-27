set exepath=%~dp0\bin\\stylegan.exe

set model=%1
set model_path=model/

if "%1"=="" set model=f

del *.png

%exepath% --seed 1629898316 --seed2 841 --num 20 --smooth_z 1 --model_path %model_path% --model %model%
goto end

goto 2
%exepath% --seed 1629898316 --seed2 841 --num 20 --smooth_z 1 --model_path %model_path% --model %model%
%exepath% --seed 841 --seed2 1629897845 --num 20 --smooth_z 1 --model_path %model_path% --model %model% --start_index 20
%exepath% --seed 1629897845 --seed2 1629898142 --num 20 --smooth_z 1 --model_path %model_path% --model %model% --start_index 40
%exepath% --seed 1629898142 --seed2 1629898169 --num 20 --smooth_z 1 --model_path %model_path% --model %model% --start_index 60
%exepath% --seed 1629898169 --seed2 1629898429 --num 20 --smooth_z 1 --model_path %model_path% --model %model% --start_index 80
%exepath% --seed 1629898429 --seed2 1629898620 --num 20 --smooth_z 1 --model_path %model_path% --model %model% --start_index 100
goto end

:2
%exepath% --seed 841 --seed2 36 --num 20 --smooth_z 1 --model_path %model_path% --model %model%
%exepath% --seed 36 --seed2 187 --num 20 --smooth_z 1 --model_path %model_path% --model %model% --start_index 20
%exepath% --seed 187 --seed2 290 --num 20 --smooth_z 1 --model_path %model_path% --model %model% --start_index 40
%exepath% --seed 290 --seed2 541 --num 20 --smooth_z 1 --model_path %model_path% --model %model% --start_index 60
%exepath% --seed 541 --seed2 561 --num 20 --smooth_z 1 --model_path %model_path% --model %model% --start_index 100
%exepath% --seed 561 --seed2 701 --num 20 --smooth_z 1 --model_path %model_path% --model %model% --start_index 120
%exepath% --seed 701 --seed2 810 --num 20 --smooth_z 1 --model_path %model_path% --model %model% --start_index 140
%exepath% --seed 810 --seed2 843 --num 20 --smooth_z 1 --model_path %model_path% --model %model% --start_index 160
%exepath% --seed 843 --seed2 995 --num 20 --smooth_z 1 --model_path %model_path% --model %model% --start_index 180
%exepath% --seed 995 --seed2 0 --num 20 --smooth_z 1 --model_path %model_path% --model %model% --start_index 200
%exepath% --seed 0 --seed2 841 --num 20 --smooth_z 1 --model_path %model_path% --model %model% --start_index 220
goto end

:end
set ffmpgpath=%~dp0\bin\ffmpeg.exe

%ffmpgpath% -i image_%%04d.png  output.gif
