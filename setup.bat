copy main.cpp stylegancpp /v /y

echo %~dp0\application\bin> .\application\bin\stylegan_path.txt
echo %~dp0\application\bin> .\styleGANapp\styleGANapp\bin\Debug\stylegan_path.txt
echo %~dp0\application\bin> .\styleGANapp\styleGANapp\bin\Release\stylegan_path.txt