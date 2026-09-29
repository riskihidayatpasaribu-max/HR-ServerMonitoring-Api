using HR.ServerMonitoring.Api.Data;
using Microsoft.AspNetCore.Mvc;
using Microsoft.EntityFrameworkCore;

namespace HR.ServerMonitoring.Api.Controllers;

[ApiController]
[Route("api/[controller]")]
public class DeviceEventsController : ControllerBase
{
    private readonly AppDbContext _context;

    public DeviceEventsController(AppDbContext context)
    {
        _context = context;
    }

    [HttpGet]
    public async Task<IActionResult> GetEvents()
    {
        var events = await _context.DeviceEvents
            .OrderByDescending(e => e.Timestamp)
            .Take(100)
            .ToListAsync();

        return Ok(events);
    }
}