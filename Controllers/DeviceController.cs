using Microsoft.AspNetCore.Mvc;
using HR.ServerMonitoring.Api.Services;

namespace HR.ServerMonitoring.Api.Controllers;

[ApiController]
[Route("api/[controller]")]
public class DeviceController : ControllerBase
{
    private readonly DeviceStatusService _deviceStatusService;

    public DeviceController(
        DeviceStatusService deviceStatusService)
    {
        _deviceStatusService = deviceStatusService;
    }

    [HttpGet]
    public IActionResult GetDevices()
    {
        return Ok(_deviceStatusService.GetDevices());
    }
}