from terrain_gen import create_degree

import srtm

downloader = srtm.SRTMDownloader(debug=True, offline=0, directory="SRTM1", cachedir="../../data")
downloader.loadFileList()

create_degree(downloader, 35, -90, "../../data", 100, "4.1")
