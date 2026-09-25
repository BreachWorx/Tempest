<?php

namespace App\Http\Controllers\Api;

use App\Http\Controllers\Controller;
use App\Helpers\NicknameGenerator;
use App\Models\ClientDevice;
use Illuminate\Http\Request;
use Illuminate\Support\Str;

class ClientDeviceController extends Controller
{
    /**
     * POST /api/client/register
     * Register a new client or update an existing one via Token ID.
     */
    public function registerClient(Request $request)
    {
        $validated = $request->validate([
            'computer_name' => 'required|string|max:255',
            'os_platform'   => 'nullable|string|max:255',
            'architecture'  => 'nullable|string|max:50',
            'status'        => 'required|in:online,offline,idle',
        ]);

        $device = null;

        // 2. If token is missing, invalid, or forged, create a fresh device
        if (!$device) {
            $device = new ClientDevice();
            $device->device_token = (string) Str::uuid();
            $device->nickname     = NicknameGenerator::generate();
        }

        // 3. Update dynamic properties (handles IP changes, host renames, status changes)
        $device->computer_name = $validated['computer_name'];
        $device->os_platform   = $validated['os_platform'] ?? $device->os_platform;
        $device->architecture  = $validated['architecture'] ?? $device->architecture;
        $device->public_ip     = $request->ip();
        $device->status       = $validated['status'];
        $device->last_seen_at = now();
        $device->save();

        return response()->json([
            'message' => 'Device registered.',
            'data'    => [
                'device_token'  => $device->device_token
            ],
        ], 200);
    }
    
    /**
     * POST /api/client/update
     * Register a new client or update an existing one via Token ID.
     */
    public function updateStatus(Request $request)
    {
        $validated = $request->validate([
            'device_token'  => 'nullable|uuid',
            'computer_name' => 'required|string|max:255',
            'os_platform'   => 'nullable|string|max:255',
            'architecture'  => 'nullable|string|max:50',
            'status'        => 'required|in:online,offline,idle',
        ]);

        $incomingToken = $validated['device_token'] ?? null;
        $device = null;

        // 1. Attempt to find device by server-issued token
        if ($incomingToken) {
            $device = ClientDevice::where('device_token', $incomingToken)->first();
        }

        // 2. If token is missing, invalid, or forged, create a fresh device
        if (!$device) {
            $device = new ClientDevice();
            $device->device_token = (string) Str::uuid();
            $device->nickname     = NicknameGenerator::generate();
        }

        // 3. Update dynamic properties (handles IP changes, host renames, status changes)
        $device->computer_name = $validated['computer_name'];
        $device->os_platform   = $validated['os_platform'] ?? $device->os_platform;
        $device->architecture  = $validated['architecture'] ?? $device->architecture;
        $device->public_ip     = $request->ip();
        $device->status       = $validated['status'];
        $device->last_seen_at = now();
        $device->save();

        return response()->json([
            'message' => 'Device status updated.',
            'data'    => [
                'device_token'  => $device->device_token,
                'nickname'      => $device->nickname,
                'computer_name' => $device->computer_name,
                'public_ip'     => $device->public_ip,
                'status'        => $device->status,
                'last_seen_at'  => $device->last_seen_at->toIso8601String(),
            ],
        ], 200);
    }

    /**
     * GET /api/clients
     * Query devices filtered by status, nickname, or token.
     */
    public function getClients(Request $request)
    {
        $query = ClientDevice::query();

        if ($request->has('device_token')) {
            $query->where('device_token', $request->query('device_token'));
        }

        if ($request->has('nickname')) {
            $query->where('nickname', $request->query('nickname'));
        }

        if ($request->has('status')) {
            $query->where('status', $request->query('status'));
        }

        $devices = $query->orderBy('last_seen_at', 'desc')->get();

        return response()->json([
            'count' => $devices->count(),
            'data'  => $devices,
        ], 200);
    }
    public function updateOnlineStatus(Request $request)
    {
        $validated = $request->validate([
            'device_token'  => 'nullable|uuid',
            'computer_name' => 'required|string|max:255',
            'status'        => 'required|in:online,offline,idle',
        ]);

        $incomingToken = $validated['device_token'] ?? null;
        $device = null;

        // 1. Attempt to find device by server-issued token
        if ($incomingToken) {
            $device = ClientDevice::where('device_token', $incomingToken)->first();
        }

        // Reject unknown or missing tokens—do NOT auto-create
        if (!$device) {
          return response()->json([
            'error'   => 'Unrecognized device token.',
            'action'  => 'register_required'
          ], 404);
        }

        // 3. Update dynamic properties (handles IP changes, host renames, status changes)
        $device->computer_name = $validated['computer_name'];
        $device->public_ip     = $request->ip();
        $device->status       = $validated['status'];
        $device->last_seen_at = now();
        $device->save();

        return response()->json([
            'message' => 'Device status updated.'
        ], 200);
    }
}
