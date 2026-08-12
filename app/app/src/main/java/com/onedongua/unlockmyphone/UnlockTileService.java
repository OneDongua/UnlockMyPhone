package com.onedongua.unlockmyphone;

import android.content.Intent;
import android.service.quicksettings.Tile;
import android.service.quicksettings.TileService;

/**
 * Quick Settings entry point for sending an unlock request.
 */
public class UnlockTileService extends TileService {
    @Override
    public void onStartListening() {
        super.onStartListening();
        Tile tile = getQsTile();
        if (tile != null) {
            tile.setState(Tile.STATE_ACTIVE);
            tile.updateTile();
        }
    }

    @Override
    public void onClick() {
        super.onClick();
        startService(new Intent(this, UnlockRequestService.class)
                .setAction(MainActivity.ACTION_UNLOCK_REQUEST));
    }
}
