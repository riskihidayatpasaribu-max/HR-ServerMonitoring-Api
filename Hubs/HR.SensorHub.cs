using Microsoft.AspNetCore.SignalR;

namespace HR.ServerMonitoring.Api.Hubs;

public class SensorHub : Hub
{
    public override async Task OnConnectedAsync()
    {
        Console.WriteLine($"HUB CONNECTED: {Context.ConnectionId}");

        await base.OnConnectedAsync();
    }

    public override async Task OnDisconnectedAsync(Exception? exception)
    {
        Console.WriteLine(
            $"HUB DISCONNECTED: {Context.ConnectionId} | {exception?.Message}");

        await base.OnDisconnectedAsync(exception);
    }
}