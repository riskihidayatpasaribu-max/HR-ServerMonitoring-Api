using Microsoft.EntityFrameworkCore;
using HR.ServerMonitoring.Api.Models; //menggunakan model SensorData

namespace HR.ServerMonitoring.Api.Data;

public class AppDbContext : DbContext
{
    public AppDbContext(DbContextOptions<AppDbContext> options) : base(options)
    {
        //ini "konstruktor" nanti diisi otomatis oleh framework
    }

    public DbSet<SensorData> SensorDataEntries { get; set; } //ini nanti akan menjadi tabel di database
    public DbSet<SensorThreshold> SensorThresholds { get; set; }
    public DbSet<DeviceEvent> DeviceEvents { get; set; }
}

