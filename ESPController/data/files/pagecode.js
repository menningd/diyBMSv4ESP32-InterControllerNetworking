const INTERNALERRORCODE =
{
    NoError:0,
    CommunicationsError:1,
    ModuleCountMismatch:2,
    TooManyModules:3
};
Object.freeze(INTERNALERRORCODE);

function identifyModule(button, cellid) {
    //Populate settings div
    $.getJSON("identifyModule.json", { c: cellid }, function (data) { }).fail(function () { $("#iperror").show(); });
}

function configureModule(button, cellid, attempts) {

    $('#loading').show();

    //Select correct row in table
    $(button).parent().parent().parent().find(".selected").removeClass("selected");
    $(button).parent().parent().addClass("selected");

    $.getJSON("modules.json", { c: cellid },
        function (data) {
            var div = $("#settingConfig .settings");
            $('#c').val(data.settings.id);

            if (data.settings.Cached == true) {
                //var bank=  Math.floor(data.settings.id / MAXIMUM_NUMBER_OF_MODULES_PER_DATA_PACKET);
                //var module= data.settings.id - (b* MAXIMUM_NUMBER_OF_MODULES_PER_DATA_PACKET);
                $("#settingConfig h2").html("Settings for module bank:" + data.settings.bank + " module:" + data.settings.module);

                //Populate settings div
                $('#ModuleId').val(data.settings.id);
                $('#Version').val(data.settings.ver);
                $('#BypassOverTempShutdown').val(data.settings.BypassOverTempShutdown);
                $('#BypassThresholdmV').val(data.settings.BypassThresholdmV);
                $('#Calib').val(data.settings.Calib.toFixed(4));
                $('#ExtBCoef').val(data.settings.ExtBCoef);
                $('#IntBCoef').val(data.settings.IntBCoef);
                $('#LoadRes').val(data.settings.LoadRes.toFixed(2));
                $('#mVPerADC').val(data.settings.mVPerADC.toFixed(2));
                $('#movetobank').val(data.settings.bank);

                $("#settingConfig").show();
                //$('#settingsForm').show();
                $('#loading').hide();
            } else {
                //Data not ready yet, we will have to try again soon
                $('#settingConfig').hide();

                if (attempts > 0) {
                    //Call back to refresh page, only try for a limited number of attempts
                    attempts--;
                    setTimeout(configureModule, 1500, button, bank, module, attempts);
                }
            }
        }).fail(function () {
            $("#iperror").show();
        });
}

function queryBMS() {

    $.getJSON("monitor.json", function (jsondata) {
        var labels = [];
        var cells = [];
        var bank = [];
        var voltages = [];
        var voltagesmin = [];
        var voltagesmax = [];
        var tempint = [];
        var tempext = [];
        var pwm = [];

        var badpktcount = [];

        var voltage = [];
        //Not currently supported
        var current = [];
        var bankmin = [];
        var bankmax = [];

        for (var bankNumber = 0; bankNumber < MAXIMUM_NUMBER_OF_BANKS; bankNumber++) {
            voltage.push(0.0);
            current.push(0.0);
            bankmin.push(5000);
            bankmax.push(0);
        }

        var minVoltage = 2.5;
        var maxVoltage = 4.5;

        for (var bankNumber = 0; bankNumber < jsondata.banks; bankNumber++) {
            //Need to cater for banks of cells
            $.each(jsondata.bank[bankNumber], function (index, value) {
                var color = value.bypass ? "#B44247" : null;

                var v = (parseFloat(value.v) / 1000.0);

                //Auto scale graph is outside of normal bounds
                if (v > maxVoltage) { maxVoltage = v; }
                if (v < minVoltage) { minVoltage = v; }

                voltages.push({ value: v, itemStyle: { color: color } });
                voltagesmin.push((parseFloat(value.minv) / 1000.0));
                voltagesmax.push((parseFloat(value.maxv) / 1000.0));

                bank.push(bankNumber);
                cells.push(index);
                badpktcount.push(value.badpkt);
                labels.push(bankNumber + "/" + index);

                color = value.bypasshot ? "#B44247" : null;
                tempint.push({ value: value.int, itemStyle: { color: color } });
                tempext.push({ value: (value.ext == -40 ? 0 : value.ext) });

                pwm.push({ value: value.pwm == 0 ? null : value.pwm });

                voltage[bankNumber] += v;
                if (value.v < bankmin[bankNumber]) { bankmin[bankNumber] = value.v; }
                if (value.v > bankmax[bankNumber]) { bankmax[bankNumber] = value.v; }
            });
        }

        //Scale down for low voltages
        if (minVoltage < 2.5) { minVoltage = 0; }


        
        //Ignore and hide any errors which are zero
        if (jsondata.monitor.badcrc == 0) { $("#badcrc").hide(); } else { $("#badcrc .v").html(jsondata.monitor.badcrc); $("#badcrc").show(); }
        if (jsondata.monitor.ignored == 0) { $("#ignored").hide(); } else { $("#ignored .v").html(jsondata.monitor.ignored); $("#ignored").show(); }
        if (jsondata.monitor.sent == 0) { $("#sent").hide(); } else { $("#sent .v").html(jsondata.monitor.sent); $("#sent").show(); }
        if (jsondata.monitor.received == 0) { $("#received").hide(); } else { $("#received .v").html(jsondata.monitor.received); $("#received").show(); }
        if (jsondata.monitor.roundtrip == 0) { $("#roundtrip").hide(); } else { $("#roundtrip .v").html(jsondata.monitor.roundtrip); $("#roundtrip").show(); }

        for (var bankNumber = 0; bankNumber < MAXIMUM_NUMBER_OF_BANKS; bankNumber++) {
            if (voltage[bankNumber] > 0) {
                $("#voltage" + (bankNumber + 1) + " .v").html(voltage[bankNumber].toFixed(2) + "V");
                var range = bankmax[bankNumber] - bankmin[bankNumber];
                $("#range" + (bankNumber + 1) + " .v").html(range + "mV");

                $("#voltage" + (bankNumber + 1)).show();
                $("#range" + (bankNumber + 1)).show();
            } else {
                $("#voltage" + (bankNumber + 1)).hide();
                $("#range" + (bankNumber + 1)).hide();
            }
        }

        //Not currently supported
        $("#current").hide();
        $("#current .v").html(current[0].toFixed(2));

        switch(jsondata.monitor.errorcode) {
        case INTERNALERRORCODE.NoError:
            $(".error").hide();
        break;

        case INTERNALERRORCODE.CommunicationsError:
            $("#commserr").show();
        break;

        case INTERNALERRORCODE.ModuleCountMismatch:
            $("#missingmodules").show();
            $("#missingmodule1").html(jsondata.monitor.modulesfnd);
            $("#missingmodule2").html(jsondata.banks*jsondata.seriesmodules);
        break;

        case INTERNALERRORCODE.TooManyModules:
            $("#toomanymodules").show();
        break;
        }


        $("#info").show();
        $("#iperror").hide();

        if ($('#modulesPage').is(':visible')) {
            var tbody = $("#modulesRows");

            if ($('#modulesRows div').length != cells.length) {
                $("#settingConfig").hide();

                //Add rows if they dont exist (or incorrect amount)
                $(tbody).find("div").remove();

                $.each(cells, function (index, value) {
                    $(tbody).append("<div><span>"
                        + bank[index]
                        + "</span><span>" + value + "</span><span></span><span class='hide'></span><span class='hide'></span>"
                        + "<span class='hide'></span><span class='hide'></span><span class='hide'></span><span class='hide'></span>"
                        + "<span><button type='button' onclick='return identifyModule(this," + index + ");'>Identify</button></span>"
                        + "<span><button type='button' onclick='return configureModule(this," + index + ",5);'>Configure</button></span></div>")
                });
            }

            var rows = $(tbody).find("div");

            $.each(cells, function (index, value) {
                var columns = $(rows[index]).find("span");
                $(columns[2]).html(voltages[index].value.toFixed(3));
                $(columns[3]).html(voltagesmin[index].toFixed(3));
                $(columns[4]).html(voltagesmax[index].toFixed(3));
                $(columns[5]).html(tempint[index].value);
                $(columns[6]).html(tempext[index].value);
                $(columns[7]).html(pwm[index].value);
                $(columns[8]).html(badpktcount[index]);
            });
        }


        if ($('#homePage').is(':visible')) {
            if (g1 == null) {
                // based on prepared DOM, initialize echarts instance
                g1 = echarts.init(document.getElementById('graph1'));

                var labelOption = { normal: { show: true, position: 'insideBottom', distance: 15, align: 'left', verticalAlign: 'middle', rotate: 90, formatter: '{c}V', fontSize: 24, color: '#eeeeee', fontFamily: 'Fira Code' } };
                var labelOption2 = { normal: { show: true, position: 'insideBottom', distance: 15, align: 'left', verticalAlign: 'middle', rotate: 90, formatter: '{c}°C', fontSize: 20, color: '#eeeeee', fontFamily: 'Fira Code' } };
                var labelOption3 = { normal: { show: true, position: 'top', distance: 5, formatter: '{c}V', fontSize: 14, color: '#c1bdbd', fontFamily: 'Fira Code' } };
                var labelOption4 = { normal: { show: true, position: 'bottom', distance: 5, formatter: '{c}V', fontSize: 14, color: '#807d7d', fontFamily: 'Fira Code' } };
                var labelOptionBypass = { normal: { show: true, position: 'bottom', distance: 5, formatter: '{c}%', fontSize: 14, color: '#807d7d', fontFamily: 'Fira Code' } };

                // specify chart configuration item and data
                var option = {
                    tooltip: {
                        show: true,
                        axisPointer: {
                            type: 'cross',
                            label: {
                                backgroundColor: '#6a7985'
                            }
                        }
                    },
                    legend: {
                        show: false
                    },
                    xAxis: [{
                        gridIndex: 0,
                        type: 'category',
                        axisLine: {
                            lineStyle: {
                                color: '#c1bdbd'
                            }
                        }
                    }, {
                        gridIndex: 1,
                        type: 'category',
                        axisLine: {
                            lineStyle: {
                                color: '#c1bdbd'
                            }
                        }
                    }],
                    yAxis: [{
                        id: 0,
                        gridIndex: 0,
                        name: 'Volts',
                        type: 'value',
                        min: 2.5,
                        max: 4.5,
                        interval: 0.25,
                        position: 'left',
                        axisLine: {
                            lineStyle: {
                                color: '#c1bdbd'
                            }
                        },
                        axisLabel: {
                            formatter: function (value, index) {
                                return value.toFixed(2);
                            }
                        }
                    },
                    {
                        id: 1,
                        gridIndex: 0,
                        name: 'Bypass',
                        type: 'value',
                        min: 0,
                        max: 100,
                        interval: 10,
                        position: 'right',
                        axisLabel: {
                            formatter: '{value}%'

                        },
                        splitLine: { show: false },
                        axisLine: {

                            lineStyle: {
                                type: 'dotted',
                                color: '#c1bdbd'
                            }
                        },
                        axisTick: {
                            show: false
                        }

                    },
                    {
                        id: 2,
                        gridIndex: 1,
                        name: 'Temperature',
                        type: 'value',
                        interval: 10,
                        position: 'left',
                        axisLine: {
                            lineStyle: {
                                color: '#c1bdbd'
                            }
                        },
                        axisLabel: {
                            formatter: '{value}°C'
                        }
                    }
                    ]
                    ,
                    series: [
                        {
                            xAxisIndex: 0,
                            name: 'Voltage',
                            yAxisIndex: 0,
                            type: 'bar',
                            data: [],
                            itemStyle: {
                                color: '#55a1ea',
                                barBorderRadius: [8, 8, 0, 0]
                            },
                            label: {
                                normal: {
                                    show: true,
                                    position: 'insideBottom',
                                    distance: 10,
                                    align: 'left',
                                    verticalAlign: 'middle',
                                    rotate: 90,
                                    formatter: '{c}V',
                                    fontSize: 24,
                                    color: '#eeeeee',
                                    fontFamily: 'Fira Code'
                                }
                            }
                        }

                        , {
                            xAxisIndex: 0,
                            name: 'Min V',
                            yAxisIndex: 0,
                            type: 'line',
                            data: [],
                            label: {
                                normal: {
                                    show: true,
                                    position: 'bottom',
                                    distance: 5,
                                    formatter: '{c}V',
                                    fontSize: 14,
                                    color: '#eeeeee',
                                    fontFamily: 'Fira Code'
                                }
                            },
                            symbolSize: 16,
                            symbol: ['circle'],
                            itemStyle: {
                                normal: {
                                    color: "#c1bdbd",
                                    lineStyle: {
                                        color: 'transparent'
                                    }
                                }
                            }
                        }


                        , {
                            xAxisIndex: 0,
                            name: 'Max V',
                            yAxisIndex: 0,
                            type: 'line',
                            data: [],
                            label: {
                                normal: {
                                    show: true,
                                    position: 'top',
                                    distance: 5,
                                    formatter: '{c}V',
                                    fontSize: 14,
                                    color: '#c1bdbd',
                                    fontFamily: 'Fira Code'
                                }
                            },
                            symbolSize: 16,
                            symbol: ['arrow'],
                            itemStyle: {
                                normal: {
                                    color: "#c1bdbd",
                                    lineStyle: {
                                        color: 'transparent'
                                    }
                                }
                            }
                        }

                        , {
                            xAxisIndex: 0,
                            name: 'Bypass',
                            yAxisIndex: 1,
                            type: 'line',
                            data: [],
                            label: {
                                normal: {
                                    show: true,
                                    position: 'right',
                                    distance: 5,
                                    formatter: '{c}%',
                                    fontSize: 14,
                                    color: '#807d7d',
                                    fontFamily: 'Fira Code'
                                }
                            },
                            symbolSize: 16,
                            symbol: ['square'],
                            itemStyle: {
                                normal: {
                                    color: "#807d7d",
                                    lineStyle: {
                                        color: 'transparent'
                                    }
                                }
                            }
                        }

                        //Temperatures
                        , {
                            xAxisIndex: 1,
                            yAxisIndex: 2,
                            name: 'BypassTemperature',
                            type: 'bar',
                            data: [],
                            itemStyle: {
                                color: '#55a1ea',
                                barBorderRadius: [8, 8, 0, 0]
                            },
                            label: {
                                normal: {
                                    show: true,
                                    position: 'insideBottom',
                                    distance: 8,
                                    align: 'left',
                                    verticalAlign: 'middle',
                                    rotate: 90,
                                    formatter: '{c}°C',
                                    fontSize: 20,
                                    color: '#eeeeee',
                                    fontFamily: 'Fira Code'
                                }
                            }
                        }

                        , {
                            xAxisIndex: 1,
                            yAxisIndex: 2,
                            name: 'CellTemperature',
                            type: 'bar',
                            data: [],
                            itemStyle: {
                                color: '#55a1ea',
                                barBorderRadius: [8, 8, 0, 0]
                            },
                            label: {
                                normal: {
                                    show: true,
                                    position: 'insideBottom',
                                    distance: 8,
                                    align: 'left',
                                    verticalAlign: 'middle',
                                    rotate: 90,
                                    formatter: '{c}°C',
                                    fontSize: 20,
                                    color: '#eeeeee',
                                    fontFamily: 'Fira Code'
                                }
                            }

                        }
                    ],
                    grid: [
                        {
                            containLabel: false,
                            left: '4%',
                            right: '4%',
                            bottom: '30%'

                        }, {
                            containLabel: false,
                            left: '4%',
                            right: '4%',
                            top: '76%'
                        }]
                };

                // use configuration item and data specified to show chart
                g1.setOption(option);
            }


            if (g1 != null) {
                g1.setOption({
                    xAxis: { data: labels },
                    yAxis: [{ gridIndex: 0, min: minVoltage, max: maxVoltage }]
                    , series: [{ name: 'Voltage', data: voltages }
                        , { name: 'Min V', data: voltagesmin }
                        , { name: 'Max V', data: voltagesmax }
                        , { name: 'Bypass', data: pwm }
                        , { name: 'BypassTemperature', data: tempint }
                        , { name: 'CellTemperature', data: tempext }]
                });
            }
        }//end homepage visible

        //Call again in a few seconds
        setTimeout(queryBMS, 4000);

        $("#loading").hide();

    }).fail(function () {
        $("#iperror").show();
        //Try again in a few seconds (2 seconds if errored)
        setTimeout(queryBMS, 2000);
        $("#loading").hide();
    });
}

$(window).on('resize', function () { if (g1 != null && g1 != undefined && $('#homePage').is(':visible')) { g1.resize(); } });


