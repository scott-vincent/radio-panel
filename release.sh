rel=v1.3.0
mkdir release >/dev/null 2>&1
rm -rf release/$rel >/dev/null 2>&1
mkdir release/$rel
cp radio-panel/radio-panel release/$rel
cp -rp radio-panel/settings release/$rel
sudo chown pi:pi release/$rel/settings/*.json
dos2unix release/$rel/settings/*.json
cp release/$rel/settings/default-settings.json release/$rel/settings/radio-panel.json
tar -zcvf release/radio-panel-$rel-raspi.tar.gz release/$rel
