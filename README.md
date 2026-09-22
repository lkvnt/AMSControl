Control system for accelerator mass-spectrometer\
PowerControl -> VCH-300\
CoolControl -> Arduino NANO + MCP2515\
SensorControl -> CAC208\
CanBusManager -> ADLINK PCI-7841\
Manager links classes together and provides turn on/off, emergency algorithms\
QtUi is the main window\
DataViewerWindow is the window for Logs\
Logger - abstract class\
LoggerTXT, LoggerCSV are inherited from Logger\
SettingsManager provides access to settings.json\
SettingsDialog show setting window depends on where it was called from\
