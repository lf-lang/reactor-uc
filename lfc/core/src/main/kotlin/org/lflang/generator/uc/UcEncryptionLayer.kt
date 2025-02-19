package org.lflang.generator.uc

import org.lflang.AttributeUtils.getLinkAttribute
import org.lflang.lf.Attribute

enum class EncryptionLayerType {
  NoEncryption
}

abstract class UcEncryptionLayer(val type: EncryptionLayerType) {
  /** Generate code calling the constructor of the source endpoint */
  abstract fun generateChannelCtorSrc(): String

  /** Generate code calling the constructor of the destination endpoint */
  abstract fun generateChannelCtorDest(): String

  abstract val codeType: String

  companion object {
    fun createEncryptionLayerForBundle(bundle: UcFederatedConnectionBundle): UcEncryptionLayer {
      val attr: Attribute? = getLinkAttribute(bundle.groupedConnections.first().lfConn)

      if (attr == null) {
        return UcNoEncryptionLayer()
      } else {
        val encryption = attr.getParamString("encryption")

        when (encryption) {
          "no_encryption" -> return UcNoEncryptionLayer()
        }

        return UcNoEncryptionLayer()
      }
    }
  }
}

class UcNoEncryptionLayer : UcEncryptionLayer(EncryptionLayerType.NoEncryption) {
  override fun generateChannelCtorSrc(): String =
      "NoEncryptionLayer_ctor(&self->encryption_layer, (NetworkChannel*)&self->channel);"

  override fun generateChannelCtorDest(): String =
      "NoEncryptionLayer_ctor(&self->encryption_layer, (NetworkChannel*)&self->channel);"

  override val codeType: String
    get() = "NoEncryptionLayer"
}
