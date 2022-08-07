
export async function get({request}) {
    return {
        status: 200,
        body: {
            "build": {
                "date": "Jul 30 2022",
                "time": "18:44:25"
            },
            "chip": {
                "cores": 2,
                "cpuFreqMHz": 240,
                "cycleCount": 2752759759,
                "efuseMac": 92372254712696,
                "model": "ESP32-D0WDQ6",
                "revision": 1,
                "sdkVersion": "v4.4-dev-3569-g6a7d83af19-dirty",
                "temperature": 49.44444444
            },
            "filesystem": {
                "totalBytes": 786432,
                "type": "LittleFS",
                "usagePercent": 68.75,
                "usedBytes": 540672
            },
            "flash": {
                "flashChipMode": 2,
                "flashChipRealSize": 4194304,
                "flashChipSize": 4194304,
                "flashChipSpeedMHz": 80,
                "sdkVersion": 4194304
            },
            "ram": {
                "freeHeap": 129688,
                "heapSize": 234856,
                "maxAllocHeap": 65524,
                "minFreeHeap": 27424,
                "usagePercent": 55.22022247
            },
            "rebootReason": 3,
            "sketch": {
                "maxSize": 1572864,
                "md5": "5a7682ea68473bb5a654352ee6bf3049",
                "size": 1158320,
                "usagePercent": 73.64399719
            },
            "spi": {
                "freePsram": 0,
                "maxAllocPsram": 0,
                "minFreePsram": 0,
                "psramSize": 0
            }
        }
        
    }
}