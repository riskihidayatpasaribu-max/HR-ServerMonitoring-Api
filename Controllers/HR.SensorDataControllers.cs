using Microsoft.AspNetCore.Mvc;
using HR.ServerMonitoring.Api.Models; //menggunakan model SensorData
using HR.ServerMonitoring.Api.Data; //menggunakan 
using Microsoft.AspNetCore.SignalR; //menggunakan SignalR
using HR.ServerMonitoring.Api.Hubs; //menggunakan SensorHub
using Microsoft.EntityFrameworkCore; //menggunakan Entity Framework Core

[ApiController]          //penerima api
[Route("api/[controller]")] //route api
public class SensorDataController : ControllerBase // class ini wajib pakai nama diakhiri 'Controller'
{
    private readonly AppDbContext _context; //membuat variabel _context untuk mengakses database
    private readonly IHubContext<SensorHub> _hubContext; //membuat variabel _hubContext untuk mengakses hub SignalR

    public SensorDataController(AppDbContext context, IHubContext<SensorHub> hubContext)
    {
        _context = context;
        _hubContext = hubContext;
    }

    [HttpPost]
    public async Task<IActionResult> ReceiveData(SensorData data)
    {
        if (data.Timestamp == default)
        {
            data.Timestamp = DateTime.UtcNow;
        }

        _context.SensorDataEntries.Add(data);

        await _context.SaveChangesAsync();

        await _hubContext.Clients.All.SendAsync(
            "ReceiveSensorData",
            data);

        Console.WriteLine(
            $"Data diterima dari: {data.DeviceId}");

        return Ok();
    }

    [HttpGet] //method get
    public IActionResult GetAllData()
    {
        var allData = _context.SensorDataEntries.ToList();
        return Ok(allData); //mengembalikan semua data dalam bentuk JSON
    }

    [HttpDelete]
    public async Task<IActionResult> DeleteAllData()
    {
        await _context.Database.ExecuteSqlRawAsync(
            "DELETE FROM SensorDataEntries");

        return Ok(new
        {
            message = "Semua data sensor berhasil dihapus."
        });
    }
}

